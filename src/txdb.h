// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "coins.h"
#include "coinsbyscript.h"
#include "dbwrapper.h"
#include "chain.h"
#include "base58.h"
#include "leveldb/db.h"
#include <stdio.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/function.hpp>

class CBlockIndex;
class CCoinsViewDBCursor;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 300;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB specific cache, if no -txindex (MiB)
static const int64_t nMaxBlockDBCache = 2;
//! Max memory allocated to block tree DB specific cache, if -txindex (MiB)
// Unlike for the UTXO database, for the txindex scenario the leveldb cache make
// a meaningful difference: https://github.com/bitcoin/bitcoin/pull/8273#issuecomment-229601991
static const int64_t nMaxBlockDBAndTxIndexCache = 1024;
//! Max memory allocated to coin DB specific cache (MiB)
static const int64_t nMaxCoinsDBCache = 8;

struct CDiskTxPos : public CDiskBlockPos
{
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CDiskBlockPos*)this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};
/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);
public:
    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &fileinfo);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool &fReindex);
    bool ReadTxIndex(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> > &list);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts(boost::function<CBlockIndex*(const uint256&)> insertBlockIndex);
};

/** CCoinsView backed by the coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{

private:
    CCoinsViewByScript* pcoinsViewByScript;

protected:
    CDBWrapper db;
public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoins(const uint256 &txid, CCoins &coins) const;
    bool GetCoinsByHashOfScript(const uint256 &hash, CCoinsByScript &coins) const;
    bool HaveCoins(const uint256 &txid) const;
    uint256 GetBestBlock() const;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock);
    CCoinsViewCursor *Cursor() const;
    bool DeleteAllCoinsByScript();   // removes txoutsbyaddressindex
    bool GenerateAllCoinsByScript(); // creates txoutsbyaddressindex
    void SetCoinsViewByScript(CCoinsViewByScript* pcoinsViewByScriptIn);
    bool GetStats(CCoinsStats &stats) const;

private:
    bool BatchWriteCoinsByScript(CDBBatch& batch, const uint256 &hash, const CCoinsByScript &coins);
};

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    ~CCoinsViewDBCursor() {}

    bool GetKey(uint256 &key) const;
    bool GetValue(CCoins &coins) const;
    unsigned int GetValueSize() const;

    bool Valid() const;
    void Next();

private:
    CCoinsViewDBCursor(CDBIterator* pcursorIn, const uint256 &hashBlockIn):
        CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    boost::scoped_ptr<CDBIterator> pcursor;
    std::pair<char, uint256> keyTmp;

    friend class CCoinsViewDB;
};


/** View on the open balance dataset. */
class CBalanceViewDB
{
private:
    //! the database itself
    leveldb::DB* pdb;

    //! database options used
    leveldb::Options options;

    //! cache for multi-transaction balance updating
    std::map<std::string, CAmount> cacheBalance;

    bool WriteDB(std::string key, int nHeight, CAmount value);

    bool ReadDB(std::string key, int nHeight, CAmount& value);

public:
    //! Constructor
    CBalanceViewDB();

    //! As we use CBalanceViews polymorphically, have a destructor
    ~CBalanceViewDB();

    //! Clear the balance cache
    void ClearCache();

    //! Retrieve the CBalance for a given address
    CAmount GetBalance(std::string address, int nHeight);

    //! Update the Balance dataset represented by view
    bool UpdateBalance(const CTransaction& tx, const CCoinsViewCache& inputs, int nHeight);
};

/** View on the open reward rate dataset. */
class CRewardRateViewDB
{
private:
    //! the database itself
    leveldb::DB* pdb;

    //! database options used
    leveldb::Options options;

    bool WriteDB(int nHeight, std::string address, double value);

    bool ReadDB(int nHeight, std::string& address_value);

public:
    //! Constructor
    CRewardRateViewDB();

    //! Destructor
    ~CRewardRateViewDB();

    //! Retrieve the reward rate for a given address
    bool GetRewardRate(int nHeight, std::string& addr_rate);

    //! Update the reward rate dataset represented by view
    bool UpdateRewardRate(std::string leaderAddress, double val, int nHeight);
};

#endif // BITCOIN_TXDB_H
