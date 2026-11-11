"""Residual proxy network with stable parameter names for checkpoint loading."""

import torch
from torch import nn


class ResidualBlock(nn.Module):
    def __init__(self, width: int) -> None:
        super().__init__()
        self.norm = nn.LayerNorm(width)
        self.first = nn.Linear(width, width * 2)
        self.second = nn.Linear(width * 2, width)

    def forward(self, values: torch.Tensor) -> torch.Tensor:
        residual = self.second(torch.nn.functional.silu(self.first(self.norm(values))))
        return values + residual


class Backbone(nn.Module):
    def __init__(
        self,
        numeric_count: int,
        width: int,
        blocks: int,
        use_year_embedding: bool = True,
    ) -> None:
        super().__init__()
        self.use_year_embedding = use_year_embedding
        self.lc_embedding = nn.Embedding(12, 4)
        self.year_embedding = nn.Embedding(13, 4) if use_year_embedding else None
        categorical_count = 4 + (4 if use_year_embedding else 0)
        self.input_feature_count = numeric_count + categorical_count
        self.input = nn.Linear(self.input_feature_count, width)
        self.blocks = nn.ModuleList([ResidualBlock(width) for _ in range(blocks)])
        self.output_norm = nn.LayerNorm(width)

    def forward(
        self, numeric: torch.Tensor, lc: torch.Tensor, year: torch.Tensor
    ) -> torch.Tensor:
        categorical = [self.lc_embedding(lc)]
        if self.year_embedding is not None:
            categorical.append(self.year_embedding(year))
        values = torch.cat((numeric, *categorical), dim=1)
        values = torch.nn.functional.silu(self.input(values))
        for block in self.blocks:
            values = block(values)
        return self.output_norm(values)


class ProxyNetwork(nn.Module):
    def __init__(
        self,
        numeric_count: int,
        width: int,
        blocks: int,
        sharing: str,
        loss_name: str,
        quantiles: list[float],
        use_year_embedding: bool = True,
    ) -> None:
        super().__init__()
        self.loss_name = loss_name
        self.quantiles = quantiles
        output_count = 1
        if loss_name in ("gaussian", "student_t"):
            output_count = 2
        elif loss_name == "quantile":
            output_count = len(quantiles)
        self.sharing = sharing
        self.use_year_embedding = use_year_embedding
        if sharing == "shared":
            self.backbone = Backbone(numeric_count, width, blocks, use_year_embedding)
            self.input_feature_count = self.backbone.input_feature_count
            self.heads = nn.ModuleList(
                [
                    nn.Sequential(
                        nn.Linear(width, 128),
                        nn.SiLU(),
                        nn.Linear(128, output_count),
                    )
                    for _ in range(2)
                ]
            )
        elif sharing == "separate":
            self.backbones = nn.ModuleList(
                [
                    Backbone(numeric_count, width, blocks, use_year_embedding)
                    for _ in range(2)
                ]
            )
            self.input_feature_count = self.backbones[0].input_feature_count
            self.heads = nn.ModuleList(
                [
                    nn.Sequential(
                        nn.Linear(width, 128),
                        nn.SiLU(),
                        nn.Linear(128, output_count),
                    )
                    for _ in range(2)
                ]
            )
        else:
            raise ValueError(f"Unknown sharing mode: {sharing}")

    def forward(
        self, numeric: torch.Tensor, lc: torch.Tensor, year: torch.Tensor
    ) -> torch.Tensor:
        outputs: list[torch.Tensor] = []
        if self.sharing == "shared":
            hidden = self.backbone(numeric, lc, year)
            outputs = [head(hidden) for head in self.heads]
        else:
            outputs = [
                head(backbone(numeric, lc, year))
                for backbone, head in zip(self.backbones, self.heads)
            ]
        stacked = torch.stack(outputs, dim=1)
        if self.loss_name == "mse":
            return stacked[:, :, 0]
        if self.loss_name == "quantile":
            median_position = self.quantiles.index(0.5)
            raw_median = stacked[:, :, median_position]
            result = torch.empty_like(stacked)
            result[:, :, median_position] = raw_median
            lower = raw_median
            for position in range(median_position - 1, -1, -1):
                lower = lower - torch.nn.functional.softplus(stacked[:, :, position])
                result[:, :, position] = lower
            upper = raw_median
            for position in range(median_position + 1, len(self.quantiles)):
                upper = upper + torch.nn.functional.softplus(stacked[:, :, position])
                result[:, :, position] = upper
            return result
        return stacked
