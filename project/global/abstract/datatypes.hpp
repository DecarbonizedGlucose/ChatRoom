#ifndef DATATYPES_HPP
#define DATATYPES_HPP

enum class DataType {
    None,
    Message,
    Command,
    FileChunk,       // in any
    SyncItem,        // in any
    OfflineMessages  // in any
};

#endif