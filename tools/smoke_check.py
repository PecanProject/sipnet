import argparse
import subprocess
from io import StringIO

import pandas as pd

parser = argparse.ArgumentParser(
    prog='smoke_check',
    description='Check the results of a smoke test directory',
    epilog='',
    usage='%(prog)s [options]')
parser.add_argument('smoke_dir')
parser.add_argument('--header', action='store_true',
                    help='use to indicate file has a header row')

def main():
  args = parser.parse_args()

  # File from recent smoke test run (presumably)
  file = args.smoke_dir + '/sipnet.out'
  # git version of file
  # git show HEAD:<file>
  git_result = subprocess.run(['git', 'show', 'HEAD:' + file], capture_output=True, text=True, check=True)
  git_result = git_result.stdout.strip()
  git_result = StringIO(git_result)

  if args.header:
    new_df = pd.read_table(file, skiprows=1, header=0, sep=r'\s+')#, dtype=float)
    git_df = pd.read_table(git_result, skiprows=1, header=0, sep=r'\s+')#, dtype=float)
    #cols = new_df.columns
  else:
    cols = 'year day time plantWoodC plantLeafC soil microbeC coarseRootC fineRootC litter litterWater soilWater soilWetnessFrac snow npp nee cumNEE gpp rAboveground rSoil rRoot ra rh rtot evapotranspiration fluxestranspiration fPAR'
    cols = cols.split(' ')
    new_df = pd.read_table(file, sep=r'\s+', header=None, names=cols, dtype=float)
    git_df = pd.read_table(git_result, sep=r'\s+', header=None, names=cols, dtype=float)

  diff_df = git_df.compare(new_df, result_names=('old', 'new'))
  print(diff_df)
  #breakpoint()

if __name__ == "__main__":
    main()
