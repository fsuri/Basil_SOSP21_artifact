'''
 Copyright 2021 Matthew Burke <matthelb@cs.cornell.edu>
                Florian Suri-Payer <fsp@cs.cornell.edu>

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use, copy,
 modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

'''
import utils
import sys
import concurrent.futures

from utils.experiment_util import *

def main():
    if len(sys.argv) != 3:
        sys.stderr.write('Usage: python3 %s <config_file> <exp_dir>\n' % sys.argv[0])
        sys.exit(1)
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
        regenerate_plots(sys.argv[1], sys.argv[2], executor)


if __name__ == "__main__":
    main()
