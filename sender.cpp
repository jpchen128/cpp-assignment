#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <chrono>

#include "property_tree_generated.h"
using namespace PropertyTree;
using namespace std;

using boost::asio::ip::tcp;

class SenderProperty {
public:
  SenderProperty(string name): name(name) {}

  string getName() {
    return name;
  }

  void setName(string name) {
    this->name = name;
  }

  bool getUpdateFlag() {
    return updateFlag;
  }

  void setUpdateFlag(bool flag) {
    updateFlag = flag;
  }

  virtual string describe() = 0;

  virtual flatbuffers::Offset<Property> serializeOnCreateOrUpdate(flatbuffers::FlatBufferBuilder& builder) = 0;

  flatbuffers::Offset<Property> serializeOnDelete(flatbuffers::FlatBufferBuilder& builder) {
    unsigned long time = chrono::system_clock::now().time_since_epoch().count();
    auto fb_name = builder.CreateString(name);
    return CreateProperty(builder, time, true, fb_name);
  }

private:
  string name;
  bool updateFlag = true;
};

template <typename T>
class LeafSenderProperty: public SenderProperty {
public:
  LeafSenderProperty(string name, T value): SenderProperty(name), value(value) {}

  T getValue() {
    return value;
  }

  void setValue(T value) {
    this->value = value;
    setUpdateFlag(true);
  }

  string getValueString() {
    stringstream ss;
    ss << value;
    return ss.str();
  }

  string describe() {
    return getName() + ": " + getValueString();
  }

  flatbuffers::Offset<Property> serializeOnCreateOrUpdate(flatbuffers::FlatBufferBuilder& builder) {
    if (!getUpdateFlag()) {
      throw runtime_error("Property not updated");
    }
    setUpdateFlag(false);
    unsigned long time = chrono::system_clock::now().time_since_epoch().count();
    auto fb_name = builder.CreateString(getName());
    auto fb_value = builder.CreateString(getValueString());
    auto fb_type = is_same<T, string>::value ? ValueType_String : \
                  (is_same<T, int>::value ? ValueType_Int : \
                  (is_same<T, float>::value ? ValueType_Float : ValueType_Bool));
    return CreateProperty(builder, time, false, fb_name, fb_value, fb_type);
  }

private:
  T value;
};

class NonLeafSenderProperty: public SenderProperty {
public:
  NonLeafSenderProperty(string name, vector<shared_ptr<SenderProperty>> subprops): SenderProperty(name), subprops(subprops) {}
  NonLeafSenderProperty(string name): SenderProperty(name) {
    subprops = vector<shared_ptr<SenderProperty>>();
  }

  void addChild(shared_ptr<SenderProperty> child) {
    subprops.push_back(child);
    setUpdateFlag(true);
  }

  void removeChild(shared_ptr<SenderProperty> child) {
    subprops.erase(remove(subprops.begin(), subprops.end(), child), subprops.end());
    setUpdateFlag(true);
  }

  string describe() {
    string result = getName() + ": {\n";
    for (auto& prop : subprops) {
      result += prop->describe() + ",\n";
    }
    result += "}";
    return result;
  }

  flatbuffers::Offset<Property> serializeOnCreateOrUpdate(flatbuffers::FlatBufferBuilder& builder) {
    if (!getUpdateFlag()) {
      throw runtime_error("Property not updated");
    }
    setUpdateFlag(false);
    unsigned long time = chrono::system_clock::now().time_since_epoch().count();
    auto fb_name = builder.CreateString(getName());
    vector<flatbuffers::Offset<Property>> fb_subprops;
    for (auto& prop : subprops) {
      if (prop->getUpdateFlag()) {
        fb_subprops.push_back(prop->serializeOnCreateOrUpdate(builder));
      }
    }
    auto fb_vector = builder.CreateVector(fb_subprops);
    return CreateProperty(builder, time, false, fb_name, 0, ValueType_Subprops, fb_vector);
  }

private:
    vector<shared_ptr<SenderProperty>> subprops;
};

int main(int argc, char* argv[]) {
  try {
    string ip;
    int port;
    if (argc == 3) {
      ip = argv[1];
      port = stoi(argv[2]);
    } else {
      cerr << "Usage: " << argv[0] << " <host> <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    socket.connect(tcp::endpoint(boost::asio::ip::address::from_string(ip), port));

    auto portfolio = make_shared<NonLeafSenderProperty>("Portfolio", 
      vector<shared_ptr<SenderProperty>>{
        make_shared<NonLeafSenderProperty>("TICKER1", 
          vector<shared_ptr<SenderProperty>>{
            make_shared<LeafSenderProperty<float>>("Price", 100.0),
            make_shared<LeafSenderProperty<string>>("Currency", "USD"),
            make_shared<LeafSenderProperty<int>>("Volume", 1000),
          }
        ),
        make_shared<NonLeafSenderProperty>("TICKER2", 
          vector<shared_ptr<SenderProperty>>{
            make_shared<LeafSenderProperty<float>>("Price", 200.0),
            make_shared<LeafSenderProperty<string>>("Currency", "EUR"),
            make_shared<LeafSenderProperty<int>>("Volume", 2000),
          }
        ),
      }
    );
    cout << portfolio->describe() << endl;

    flatbuffers::FlatBufferBuilder builder;
    auto fb_portfolio = portfolio->serializeOnCreateOrUpdate(builder);
    builder.Finish(fb_portfolio);

    // 3. Write code snippets that can send/receive the property object over TCP socket.
    auto msg_ptr = builder.GetBufferPointer();
    auto msg_size = builder.GetSize();
    boost::asio::const_buffer msg = boost::asio::buffer(static_cast<const void*>(msg_ptr), msg_size);
    boost::system::error_code error;
    boost::asio::write(socket, msg, error);
  } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
