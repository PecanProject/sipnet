#!/bin/bash

echo "Setting up pre-commit hook"
ln -s tools/clang-format.hook .git/hooks/pre-commit

echo "Setup complete"
