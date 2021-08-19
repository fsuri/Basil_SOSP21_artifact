/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
#include "store/apps/example/exampletransaction.h"

ExampleTransaction::ExampleTransaction() {
}

ExampleTransaction::~ExampleTransaction() {
}

/**
 * Begin()
 * v = Get('a') + Get('b') + Get('c)
 * Put('d', v)
 * Commit()
 */

OperationType ExampleTransaction::GetNextOperationType() {
  switch (GetOpCount()) {
    case 0:
      return GET;
    case 1:
      return GET;
    case 2:
      return GET;
    case 3:
      return PUT;
    case 4:
      return COMMIT;
    default:
      return DONE;
  }
}

void ExampleTransaction::GetNextOperationKey(std::string &key) {
  switch (GetOpCount()) {
    case 0:
      key = "a";
      break;
    case 1:
      key = "b";
      break;
    case 2:
      key = "c";
      break;
    case 3:
      key = "d";
      break;
    default:
      break;
  }
}

void ExampleTransaction::GetNextPutValue(std::string &value) {
  std::string rv;
  bool found;

  value = "";

  GetReadValue("a", rv, found);
  UW_ASSERT(found);
  value = value + rv;
  
  GetReadValue("b", rv, found);
  UW_ASSERT(found);
  value = value + rv;

  GetReadValue("c", rv, found);
  UW_ASSERT(found);
  value = value + rv;
}

void ExampleTransaction::OnOperationCompleted(const Operation *op,
    ::Client *client) {
}

void ExampleTransaction::CopyStateInto(AsyncTransaction *txn) const {
}
