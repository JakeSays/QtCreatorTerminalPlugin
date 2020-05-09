#pragma once

#include <QMap>
#include <QHash>

template<typename TKey, typename TValue>
class QMapAdapter
{
    QMap<TKey, TValue> _map;

public:
    typedef QMap<TKey, TValue> MapType;
    typedef typename MapType::key_value_iterator iterator;
    typedef typename MapType::const_key_value_iterator const_iterator;

    QMapAdapter(QMap<TKey, TValue> map)
        : _map(map)
    {}

    iterator begin() { return _map.keyValueBegin(); }
    iterator end() { return _map.keyValueEnd(); }
    const_iterator cbegin() const { return _map.constKeyValueBegin(); }
    const_iterator cend() const { return _map.constKeyValueEnd(); }
};

template<typename TKey, typename TValue>
class QHashAdapter
{
    QHash<TKey, TValue> _hash;

public:
    typedef QHash<TKey, TValue> HashType;
    typedef typename HashType::key_value_iterator iterator;
    typedef typename HashType::const_key_value_iterator const_iterator;

    QHashAdapter(QHash<TKey, TValue> hash)
        : _hash(hash)
    {}

    iterator begin() { return _hash.keyValueBegin(); }
    iterator end() { return _hash.keyValueEnd(); }
    const_iterator cbegin() const { return _hash.constKeyValueBegin(); }
    const_iterator cend() const { return _hash.constKeyValueEnd(); }
};
