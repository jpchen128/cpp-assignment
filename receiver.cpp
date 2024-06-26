#include <boost/asio.hpp>
#include <iostream>
#include <unordered_map>

#include "property_tree_generated.h"
using namespace PropertyTree;
using namespace std;

using boost::asio::ip::tcp;

class ReceiverProperty {
public:
  ReceiverProperty(time_t timestamp): timestamp() {}

  time_t getTimestamp() {
    return timestamp;
  }

  void setTimestamp(time_t timestamp) {
    this->timestamp = timestamp;
  }

  virtual string getValueString() = 0;
private:
  time_t timestamp;
};

template <typename T>
class LeafReceiverProperty : public ReceiverProperty {
public:
  LeafReceiverProperty(time_t timestamp): ReceiverProperty(timestamp) {}

  T getValue() {
    return value;
  }

  void setValue(T value) {
    this->value = value;
  }

  string getValueString() {
    stringstream ss;
    ss << value;
    return ss.str();
  }
private:
  T value;
};

// 2. Read/update on the property object generated by the FlatBuffers compiler.
class NonLeafReceiverProperty : public ReceiverProperty {
public:
  NonLeafReceiverProperty(time_t timestamp): ReceiverProperty(timestamp) {}

  void addOrUpdateSubprop(const Property* property) {
    auto name = property->name()->str();
    if (property->deleted()) {
      subprops.erase(name);
      return;
    }
    if (subprops.find(name) == subprops.end()) {
      switch (property->type()) {
        case ValueType_String:
          subprops[name] = make_shared<LeafReceiverProperty<string>>(property->timestamp());
          break;
        case ValueType_Int:
          subprops[name] = make_shared<LeafReceiverProperty<int>>(property->timestamp());
          break;
        case ValueType_Float:
          subprops[name] = make_shared<LeafReceiverProperty<float>>(property->timestamp());
          break;
        case ValueType_Bool:
          subprops[name] = make_shared<LeafReceiverProperty<bool>>(property->timestamp());
          break;
        case ValueType_Subprops:
          subprops[name] = make_shared<NonLeafReceiverProperty>(property->timestamp());
          break;
      }
    }
    switch (property->type()) {
      case ValueType_String:
        reinterpret_cast<LeafReceiverProperty<string>*>(subprops[name].get())->setValue(property->value()->str());
        break;
      case ValueType_Int:
        reinterpret_cast<LeafReceiverProperty<int>*>(subprops[name].get())->setValue(stoi(property->value()->str()));
        break;
      case ValueType_Float:
        reinterpret_cast<LeafReceiverProperty<float>*>(subprops[name].get())->setValue(stof(property->value()->str()));
        break;
      case ValueType_Bool:
        reinterpret_cast<LeafReceiverProperty<bool>*>(subprops[name].get())->setValue(property->value()->str() == "true");
        break;
      case ValueType_Subprops:
        for (int i = 0; i < property->sub_properties()->size(); i++) {
          reinterpret_cast<NonLeafReceiverProperty*>(subprops[name].get())->addOrUpdateSubprop(property->sub_properties()->Get(i));
        }
        break;
    }
  }

  string getValueString() {
    stringstream ss;
    ss << "{" << endl;
    for (auto& prop : subprops) {
      ss << "\"" << prop.first << "\": " << prop.second->getValueString() << "," << endl;
    }
    ss << "}";
    return ss.str();
  }
private:
  unordered_map<string, shared_ptr<ReceiverProperty>> subprops;
};

class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket socket)
    : socket_(std::move(socket)),
      rootprop(chrono::system_clock::to_time_t(chrono::system_clock::now())) {}

  void start() {
    do_read();
  }

private:
  // 4. Use the reflection API to read from the TCP socket and iterate over the elements stored inside the property tree.
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
          auto property = GetProperty(data_);
          rootprop.addOrUpdateSubprop(property);
          cout << rootprop.getValueString() << endl;
        }
      });
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
  NonLeafReceiverProperty rootprop;
};

class Receiver {
public:
  Receiver(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    do_accept();
  }

private:
  void do_accept() {
    acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
          std::make_shared<Session>(std::move(socket))->start();
        }
        do_accept();
      });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
  try {
    int port;
    if (argc == 2) {
      port = stoi(argv[1]);
    } else {
      cerr << "Usage: " << argv[0] << " <port>\n";
      return 1;
    }
    boost::asio::io_context io_context;
    Receiver s(io_context, port);
    io_context.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
