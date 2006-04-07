function [vpress, vpd, vpdsoil] = calcvpress(rhum, tair, tsoil)
% Given filled vectors of rhum, tair and tsoil (all same length),
% calculate & return the air vapor pressure,
% vpd in air, and vpd in soil

esatair = zeros(size(rhum));
esatair = 611*exp((2.5e6/461.5)*((1./273.15)-(1./(273.15+tair))));
esatsoil = zeros(size(rhum));
esatsoil = 611*exp((2.5e6/461.5)*((1./273.15)-(1./(273.15+tsoil))));

vpress = zeros(size(rhum));
vpress = rhum .* esatair/100;

vpd = esatair - vpress;
vpdsoil = esatsoil - vpress;