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
#ifndef TABLES_H
#define TABLES_H

namespace tpcc {

enum WarehouseTable {
  W_ID = 0,
  W_NAME,
  W_STREET_1,
  W_STREET_2,
  W_CITY,
  W_STATE,
  W_ZIP,
  W_TAX,
  W_YTD,
};

enum DistrictTable {
  D_ID = 0,
  D_W_ID,
  D_NAME,
  D_STREET_1,
  D_STREET_2,
  D_CITY,
  D_STATE,
  D_ZIP,
  D_TAX,
  D_YTD,
  D_NEXT_O_ID,
};

enum CustomerTable {
  C_ID = 0,
  C_D_ID,
  C_W_ID,
  C_FIRST,
  C_MIDDLE,
  C_LAST,
  C_STREET_1,
  C_STREET_2,
  C_CITY,
  C_STATE,
  C_ZIP,
  C_PHONE,
  C_SINCE,
  C_CREDIT,
  C_CREDIT_LIM,
  C_DISCOUNT,
  C_BALANCE,
  C_YTD_PAYMENT,
  C_PAYMENT_CNT,
  C_DELIVERY_CNT,
  C_DATA,
};

enum HistoryTable {
  H_C_ID = 0,
  H_C_D_ID,
  H_C_W_ID,
  H_D_ID,
  H_W_ID,
  H_DATE,
  H_AMOUNT,
  H_DATA,
};

enum NewOrderTable {
  NO_O_ID = 0,
  NO_D_ID,
  NO_W_ID,
};

enum OrderTable {
  O_ID = 0,
  O_D_ID,
  O_W_ID,
  O_C_ID,
  O_ENTRY_D,
  O_CARRIER_ID,
  O_OL_CNT,
  O_ALL_LOCAL,
};

enum OrderLineTable {
  OL_O_ID = 0,
  OL_D_ID,
  OL_W_ID,
  OL_NUMBER,
  OL_I_ID,
  OL_SUPPLY_W_ID,
  OL_DELIVERY_D,
  OL_QUANTITY,
  OL_AMOUNT,
  OL_DIST_INFO,
};

enum ItemTable {
  I_ID = 0,
  I_IM_ID,
  I_NAME,
  I_PRICE,
  I_DATA,
};

enum StockTable {
  S_I_ID = 0,
  S_W_ID,
  S_QUANTITY,
  S_DIST_01,
  S_DIST_02,
  S_DIST_03,
  S_DIST_04,
  S_DIST_05,
  S_DIST_06,
  S_DIST_07,
  S_DIST_08,
  S_DIST_09,
  S_DIST_10,
  S_YTD,
  S_ORDER_CNT,
  S_REMOTE_CNT,
  S_DATA,
};

}

#endif /* TABLES_H */
