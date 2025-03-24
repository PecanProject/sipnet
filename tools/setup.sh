#!/bin/bash

echo "Setting up pre-commit hook"
cp tools/clang-format.hook .git/hooks/pre-commit

echo "Setup complete"
