/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include <gflags/gflags.h>
#include "store/benchmark/async/smallbank/smallbank_generator.h"
DEFINE_int32(num_customers, 18000, "Number of customers");
DEFINE_int32(base_balance, 1000, "Base balance (checking, saving)");
DEFINE_int32(balance_deviation, 50, "Balance deviation (checking, saving)");
DEFINE_int32(min_name_length, 8, "Minimum name length");
DEFINE_int32(max_name_length, 16, "Maximum name length");

int main(int argc, char *argv[]) {
    gflags::SetUsageMessage(
            "generates a file containing key-value pairs of Smallbank table data\n");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    smallbank::Queue<std::pair<std::string, std::string>> q(2e9);
    smallbank::Queue<std::string> names(2e9);
    std::cerr << "Generating " << FLAGS_num_customers << " customers." << std::endl;
    smallbank::SmallbankGenerator generator;
    generator.GenerateTables(q, names, FLAGS_num_customers, FLAGS_min_name_length, FLAGS_max_name_length, FLAGS_base_balance, FLAGS_balance_deviation);
    std::pair<std::string, std::string> out;
    std::string nameOut;

    int count = 0;
    std::ofstream f;
    f.open("smallbank_data");
    if (f.is_open())
    {
        while (!q.IsEmpty()) {
            q.Pop(out);
            WriteBytesToStream(&f, out.first);
            WriteBytesToStream(&f, out.second);
            count++;
        }
        f.close();
    }
    else std::cerr << "Unable to open file";

    f.open("smallbank_names");
    if (f.is_open())
    {
        while (!names.IsEmpty()) {
            names.Pop(nameOut);
            f << nameOut + ",";
        }
        f.close();
    }
    else std::cerr << "Unable to open file";

    return 0;
}
