#ifndef TS_NET_CONNECTOR_H_
#define TS_NET_CONNECTOR_H_

namespace tigerso {

class Connector: public std::enable_shared_from_this<Connector> {

public:
    Connector(const ConnectionType&);
    int addIntoFactory(std::shared_ptr<ConnectionFactory>&);
    std::shared_ptr<Connector> createConnectionTo();
        


};


} //namespace tigerso


#endif
