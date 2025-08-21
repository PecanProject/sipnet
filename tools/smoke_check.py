import os
import subprocess
import sys
from io import StringIO

import pandas as pd


def print_sep(title: str):
  sep_line = '*' * len(title)
  print('')
  print(sep_line)
  print(title)
  print(sep_line)


def get_test_dirs() -> list[str]:
  script_path = os.path.abspath(__file__)
  script_directory = os.path.dirname(script_path)
  os.chdir(script_directory + '/../tests/smoke')
  dirs = []
  for entry in os.listdir('.'):
    if os.path.isdir(entry):
      dirs.append(entry)

  return sorted(dirs)


def print_usage():
  print(f"usage: smoke_check [command] [options]")
  print(f"")
  print(f"Commands:")
  print(f"  help:  show this message and exit")
  print(f"  list:  list available smoke tests and exit")
  print(f"  run :  compare results from one or more smoke tests; see below")
  print(f"")
  print(f"Run syntax: smoke_check run [verbose] [tests]")
  print(f"  verbose: print the difference dataframe for each comparison")
  print(f"  tests  : list the tests to check; no list or 'all' runs all comparisons")
  print(f"")
  print(f"'smoke_check' with no arguments is equivalent to 'smoke_check run'")
  print(f"")
  print(f"Note: smoke_check assumes tests/smoke/run_smoke.sh has already been run")

def main():
  commands: list[str] = ['run', 'list', 'help', '-h', '--help']
  verbose: bool = False

  test_dirs = get_test_dirs()
  args = sys.argv[1:]
  run_dirs: list[str] = []

  if len(args) < 1:
    command = 'run'
  else:
    command = args[0]
    args = args[1:]
    if command not in commands:
      print(f'Error: unknown command {command}')
      print_usage()
      sys.exit(1)

  match command:
    case 'help' | '-h' | '--help':
      print_usage()
      sys.exit(0)
    case 'list':
      print('The following smoke tests exist:')
      print(', '.join(test_dirs))
      sys.exit(0)
    case 'run':
      if len(args) > 0 and args[0].lower() == 'verbose':
        verbose = True
        args = args[1:]
      if len(args) < 1 or args[0].lower() == 'all':
        run_dirs = test_dirs
      else:
        for input_dir in args:
          if input_dir in test_dirs:
            run_dirs.append(input_dir)
          else:
            print(f'Unknown input test {input_dir}; skipping')

  # Change to the given directory
  print(f'Running tests: {", ".join(run_dirs)}')
  print(f"Changed working directory to: {os.getcwd()}")

  # List subdirectories
  for test_dir in run_dirs:
    skip_file = os.path.join(test_dir, 'skip')

    # Only process if "skip" file does not exist
    if not os.path.exists(skip_file):
      check_results(test_dir, verbose)
    else:
      print_sep(f"Skipping {test_dir} (found 'skip' file)")


def check_results(smoke_dir: str, verbose: bool):
  print_sep(f'Running test {smoke_dir}')

  # File from recent smoke test run (presumably)
  file = smoke_dir + '/sipnet.out'

  # git version of file
  # git show HEAD:<file>
  git_result = subprocess.run(['git', 'show', 'HEAD:' + './' + file], capture_output=True, text=True, check=True)
  git_result = git_result.stdout.strip()
  git_result = StringIO(git_result)

  # Figure out if there is a header
  with open(file) as res_file:
    # Check first row
    first = res_file.readline()
    if first.split()[0].startswith('Notes'):
      has_header = True
    else:
      has_header = False

  if has_header:
    new_df = pd.read_table(file, skiprows=1, header=0, sep=r'\s+', dtype=float)
    git_df = pd.read_table(git_result, skiprows=1, header=0, sep=r'\s+', dtype=float)
  else:
    cols = 'year day time plantWoodC plantLeafC soil microbeC coarseRootC fineRootC litter litterWater soilWater soilWetnessFrac snow npp nee cumNEE gpp rAboveground rSoil rRoot ra rh rtot evapotranspiration fluxestranspiration fPAR'
    cols = cols.split(' ')
    new_df = pd.read_table(file, sep=r'\s+', header=None, names=cols, dtype=float)
    git_df = pd.read_table(git_result, sep=r'\s+', header=None, names=cols, dtype=float)

  diff_df = git_df.compare(new_df, result_names=('old', 'new'))

  if diff_df.empty:
    print("No differences found")
    return

  # There are differences, show info
  if verbose:
    print(diff_df)

  diff_cols = [a[0] for a in diff_df.columns][0::2]
  data = pd.DataFrame(columns=diff_cols, index=['first diff', 'total diffs', 'old mean', 'new mean'])

  for col in diff_cols:
    col_data = diff_df[col]
    col_data_old = col_data['old']
    data.loc['first diff', col] = col_data_old.first_valid_index()
    data.loc['total diffs', col] = col_data_old.count()
    means = col_data.mean()
    data.loc['old mean', col] = means.loc['old']
    data.loc['new mean', col] = means.loc['new']

  print('Difference Summary:')
  print(data)

if __name__ == "__main__":
  main()
