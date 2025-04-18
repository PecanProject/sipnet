#!/bin/bash

# SETUP STUFF
python_ver() {
  local py_ref=$1
  $py_ref -c 'import platform; major, minor, patch = platform.python_version_tuple(); print(major, minor)'
}

read -r -d '' py_error <<- EOF
Please install python version 3.8 or later and then rerun this script.
EOF

read -r -d '' linux <<- EOF
For linux OS, the following packages need to be installed. Please try the"
commands below, and then update this script to automate the process (either"
with these commands, or with whatever adjustments needed to be made)"
$ sudo apt-get update
$ sudo apt-get install -y clang-format-19 clang-tidy-19
If that doesn't work, this might:
$ wget https://apt.llvm.org/llvm.sh -O llvm_install.sh
$ chmod +x llvm_install.sh
$ if sudo llvm_install.sh 19; then
$   sudo apt-get install -y clang-format-19 clang-tidy-19
$ fi
EOF

## PYTHON CHECK
echo "Checking python installation"
min_python_major=3
min_python_minor=8
python_ref=$(which python3 | grep "/python3")
if [[ -z "$python_ref" ]]; then
  python_ref=$(which python | grep "/python")
fi
if [[ -z "$python_ref" ]]; then
  echo "No python installation found."
  echo "$py_error"
  exit 1
fi
#version=$(python_ver "$python_ref")
#major=${version[0]}; echo major $major
#minor=${version[1]}; echo minor $minor
read major minor < <(python_ver "$python_ref")
if [[ "$major" -ge "$min_python_major" && "$minor" -ge "$min_python_minor" ]]; then
  echo "Acceptable python version ($major.$minor) found"
else
  echo "Python version ($major.$minor) is too old."
  echo "$py_error"
  exit 1
fi

## CLANG TOOLS CHECK
echo "Checking clang-tools installation"
found_format=
found_tidy=
if clang-format --version >/dev/null 2>&1; then
  found_format=1;
else
  echo "clang-format not found"
fi
if clang-tidy --version >/dev/null 2>&1; then
  found_tidy=1;
else
  echo "clang-tidy not found"
fi

if [[ ! "$found_format" || ! "$found_tidy" ]]; then
  if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Installing clang-format/tidy via llvm"
    brew install llvm
    ln -s "$(brew --prefix llvm)/bin/clang-format" "/usr/local/bin/clang-format"
    ln -s "$(brew --prefix llvm)/bin/clang-tidy" "/usr/local/bin/clang-tidy"
    ln -s "$(brew --prefix llvm)/bin/git-clang-format" "/usr/local/bin/git-clang-format"
  else
    echo "$linux"
  fi
else
  echo "All clang tools found"
fi

## PRE_COMMIT HOOK INSTALL
echo "Setting up pre-commit hook"
cp tools/clang-format.hook .git/hooks/pre-commit

echo "Setup complete"
