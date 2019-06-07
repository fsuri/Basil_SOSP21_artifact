#ifndef TPCC_SCHEMA_H
#define TPCC_SCHEMA_H

namespace tpcc {

const static unsigned int kDefaultColSize = 64;

enum {
  DELIVERY,
  NEW_ORDER,
  ORDER_STATUS,
  PAYMENT,
  STOCK
} TRANSACTIONS;

// Names of the various tables and number of columns
const static std::string kWarehouseTable = "warehouse";
const static unsigned int kWarehouseCols = 9;
const static std::string kDistrictTable = "district";
const static unsigned int kDistrictCols = 11;
const static std::string kCustomerTable = "customer";
const static unsigned int kCustomerCols = 21;
const static std::string kHistoryTable = "history";
const static unsigned int kHistoryCols = 8;
const static std::string kNewOrderTable = "neworder";
const static unsigned int kNewOrderCols = 3;
const static std::string kEarliestNewOrderTable = "earliestneworder";
const static unsigned int kEarliestNewOrderCols = 3;
const static std::string kOrderTable = "order";
const static unsigned int kOrderCols = 8;
const static std::string kOrderByCustomerTable = "orderbycust";
const static unsigned int kOrderByCustomerCols = 1;
const static std::string kOrderLineTable = "orderline";
const static unsigned int kOrderLineCols = 10;
const static std::string kItemTable = "item";
const static unsigned int kItemCols = 5;
const static std::string kStockTable = "stock";
const static unsigned int kStockCols = 17;
const static std::string kItemStatsTable = "itemstats";
const static unsigned int kItemStatsCols = 1;

typedef uint8_t TableId;
typedef uint64_t RowId;

class DatabaseKey {

 public:

  virtual ~DatabaseKey() {

  }

  virtual RowId value() = 0;

  std::string StrValue(unsigned int * size) {
    std::string s = std::to_string(value());
    *size = s.length() + 1;
    return s;
  }

  std::string StrValue() {
    return std::to_string(value());
  }

};

struct WarehouseKey : public DatabaseKey {
  RowId W_ID;
  RowId value() {
    return W_ID;
  }
};

struct DistrictKey : public DatabaseKey {
  RowId D_W_ID;
  RowId D_ID;
  RowId value() {
    return (D_W_ID << 5) | D_ID;
  }
};

struct CustomerKey : public DatabaseKey {
  RowId C_W_ID;
  RowId C_D_ID;
  RowId C_ID;
  RowId value() {
    return (C_W_ID << 22) | (C_D_ID << 17) | C_ID;
  }
};

struct HistoryKey : public DatabaseKey {
};

struct NewOrderKey : public DatabaseKey {
  RowId NO_W_ID;
  RowId NO_D_ID;
  RowId NO_O_ID;
  RowId value() {
    return (NO_W_ID << 29) | (NO_D_ID << 24) | NO_O_ID;
  }
};

struct EarliestNewOrderKey : public DatabaseKey {
  RowId NO_W_ID;
  RowId NO_D_ID;
  RowId value() {
    return (NO_W_ID << 5) | NO_D_ID;
  }
};

struct OrderKey : public DatabaseKey {
  RowId O_W_ID;
  RowId O_D_ID;
  RowId O_ID;
  RowId value() {
    return (O_W_ID << 29) | (O_D_ID << 24) | O_ID;
  }
};

// latest order from a particular user
struct OrderByCustomerKey : public DatabaseKey {
  RowId O_W_ID;
  RowId O_D_ID;
  RowId O_C_ID;
  RowId value() {
    return (O_W_ID << 22) | (O_D_ID << 17) | O_C_ID;
  }
};

struct OrderLineKey : public DatabaseKey {
  RowId OL_W_ID;
  RowId OL_D_ID;
  RowId OL_O_ID;
  RowId OL_ID;
  RowId value() {
    return (OL_W_ID << 33) | (OL_D_ID << 28) | (OL_O_ID << 4) | OL_ID;
  }
};

struct ItemKey : public DatabaseKey {
  RowId I_ID;
  RowId value() {
    return I_ID;
  }
};

struct StockKey : public DatabaseKey {
  RowId S_W_ID;
  RowId S_I_ID;
  RowId value() {
    return (S_W_ID << 18) | S_I_ID;
  }
};

} // namespace tpcc

#endif /* TPCC_SCHEMA_H */
