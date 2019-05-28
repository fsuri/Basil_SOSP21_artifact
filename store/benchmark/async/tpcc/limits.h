#ifndef LIMITS_H
#define LIMITS_H

#include <stdint.h>

namespace tpcc {
namespace limits {

struct Item {
  static const int MIN_IM = 1;
  static const int MAX_IM = 10000;
  static constexpr float MIN_PRICE = 1.00;
  static constexpr float MAX_PRICE = 100.00;
  static const int MIN_NAME = 14;
  static const int MAX_NAME = 24;
  static const int MIN_DATA = 26;
  static const int MAX_DATA = 50;
  static const int NUM_ITEMS = 100000;
};

struct Warehouse {
  static constexpr float MIN_TAX = 0;
  static constexpr float MAX_TAX = 0.2000f;
  static constexpr float INITIAL_YTD = 300000.00f;
  static const int MIN_NAME = 6;
  static const int MAX_NAME = 10;
  // TPC-C 1.3.1 (page 11) requires 2*W. This permits testing up to 50 warehouses. This is an
  // arbitrary limit created to pack ids into integers.
  static const int MAX_WAREHOUSE_ID = 100;
};

struct District {
  static constexpr float MIN_TAX = 0;
  static constexpr float MAX_TAX = 0.2000f;
  static constexpr float INITIAL_YTD = 30000.00;  // different from Warehouse
  static const int INITIAL_NEXT_O_ID = 3001;
  static const int MIN_NAME = 6;
  static const int MAX_NAME = 10;
  static const int NUM_PER_WAREHOUSE = 10;
};

struct Stock {
  static const int MIN_QUANTITY = 10;
  static const int MAX_QUANTITY = 100;
  static const int DIST = 24;
  static const int MIN_DATA = 26;
  static const int MAX_DATA = 50;
  static const int NUM_STOCK_PER_WAREHOUSE = 100000;
};

struct Customer {
  static constexpr float INITIAL_CREDIT_LIM = 50000.00;
  static constexpr float MIN_DISCOUNT = 0.0000;
  static constexpr float MAX_DISCOUNT = 0.5000;
  static constexpr float INITIAL_BALANCE = -10.00;
  static constexpr float INITIAL_YTD_PAYMENT = 10.00;
  static const int INITIAL_PAYMENT_CNT = 1;
  static const int INITIAL_DELIVERY_CNT = 0;
  static const int MIN_FIRST = 6;
  static const int MAX_FIRST = 10;
  static const int MIDDLE = 2;
  static const int MAX_LAST = 16;
  static const int PHONE = 16;
  static const int CREDIT = 2;
  static const int MIN_DATA = 300;
  static const int MAX_DATA = 500;
  static const int NUM_PER_DISTRICT = 3000;
};

struct Order {
  static const int MIN_CARRIER_ID = 1;
  static const int MAX_CARRIER_ID = 10;
  // HACK: This is not strictly correct, but it works
  static const int NULL_CARRIER_ID = 0;
  // Less than this value, carrier != null, >= -> carrier == null
  static const int NULL_CARRIER_LOWER_BOUND = 2101;
  static const int MIN_OL_CNT = 5;
  static const int MAX_OL_CNT = 15;
  static const int INITIAL_ALL_LOCAL = 1;
  static const int INITIAL_ORDERS_PER_DISTRICT = 3000;
  // See TPC-C 1.3.1 (page 15)
  static const int MAX_ORDER_ID = 10000000;
};

struct OrderLine {
  static const int MIN_I_ID = 1;
  static const int MAX_I_ID = 100000;  // Item::NUM_ITEMS
  static const int INITIAL_QUANTITY = 5;
  static constexpr float MIN_AMOUNT = 0.01f;
  static constexpr float MAX_AMOUNT = 9999.99f;
  // new order has 10/1000 probability of selecting a remote warehouse for ol_supply_w_id
  static const int REMOTE_PROBABILITY_MILLIS = 10;
  static const int MAX_OL_QUANTITY = 10;
};

struct NewOrder {
  static const int INITIAL_NUM_PER_DISTRICT = 900;
};

struct History {
  static const int MIN_DATA = 12;
  static const int MAX_DATA = 24;
  static constexpr float INITIAL_AMOUNT = 10.00f;
};

} /* namespace limits */
} /* namespace tpcc */

#endif
