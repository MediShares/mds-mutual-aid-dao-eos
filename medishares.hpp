#include <functional>
#include <string>
#include <cmath>
#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/asset.hpp>

#define KEY_SYMBOL S(0,KEY)
#define STAKE_SYMBOL S(0,SKEY)
#define KEYCORE_SYMBOL S(4,KEYCORE)

#define EOS_SYMBOL S(4,EOS)

#define KEY_INIT_SUPPLY 100000000000000

using namespace eosio;
using std::string;
using namespace std;

typedef double real_type;
struct transfer_args
{
    account_name from;
    account_name to;
    asset quantity;
    string memo;
};

class medishares: public eosio::contract{
  public:
    medishares(account_name self):
    contract(self),
    global(_self, _self),
    keymarket(_self, _self),
    cases(_self, _self),
    accounts(_self, _self)
    {}

    ///@abi action
    void init(uint64_t guarantee_rate, uint64_t ref_rate, asset max_claim, time time_for_observation, time time_for_announcement, time min_apply_interval, time time_for_vote);

    ///@abi action
    void transfer(account_name from, account_name to, asset quantity, string memo);

    ///@abi action
    void sellkey(account_name account, asset key_quantity);

    ///@abi action
    void stakekey(account_name account, asset key_quantity);

    ///@abi action
    void unstakekey(account_name account, asset key_quantity);

    ///@abi action
    void propose(account_name proposer, checksum256 case_digest, asset required_fund);

    ///@abi action
    void approve(account_name account, uint64_t case_id);

    ///@abi action
    void unapprove(account_name account, uint64_t case_id);

    ///@abi action
    void cancelvote(account_name account, uint64_t case_id);

    ///@abi action
    void execproposal(account_name account, uint64_t case_id);

    ///@abi action
    void delproposal(account_name account, uint64_t case_id);

    inline asset get_balance(account_name owner, symbol_name sym)const;

    void handleTransfer(const account_name from, const account_name to, const asset& quantity, string memo);

  private:
    ///@abi table
    struct keymarket {
        asset    supply;

        struct connector {
            asset balance;
            double weight = .5;

            EOSLIB_SERIALIZE( connector, (balance)(weight) )
        };

        connector base;
        connector quote;

        uint64_t primary_key()const { return supply.symbol; }

        asset convert_to_exchange( connector& c, asset in );
        asset convert_from_exchange( connector& c, asset in );
        asset convert( asset from, symbol_type to );

        EOSLIB_SERIALIZE( keymarket, (supply)(base)(quote) )
    };

    eosio::multi_index<N(keymarket), keymarket> keymarket;

    struct asset_entry{
        asset    balance;          //KEY:可用数KEY数，SKEY:冻结KEY数，EOS:保障余额

        friend bool operator == ( const asset_entry& a, const asset_entry& b ) {
            return a.balance.symbol.name() == b.balance.symbol.name();
        }
    };

    bool has_balance(account_name owner, asset currency);
    void sub_balance(account_name owner, asset value);
    void add_balance(account_name owner, asset value, account_name ram_payer);

    struct vote_entry{
        uint64_t case_id;  //互助项目编号
        uint8_t  agreed;   //赞成或反对，1:赞成, 0:反对

        friend bool operator == ( const vote_entry& a, const vote_entry& b ) {
            return a.case_id == b.case_id;
        }
    };

    ///@abi table
    struct accounts {
        account_name    account;          //账户名
        time            join_time = 0;    //加入互助保障时间
        time            latest_apply_time = 0;    //最近申请互助时间
        vector<asset_entry> asset_list;   //资产列表
        vector<vote_entry> vote_list;     //投票列表

        uint64_t primary_key()const {return account;}

        EOSLIB_SERIALIZE(accounts, (account)(join_time)(latest_apply_time)(asset_list)(vote_list));
    };

    eosio::multi_index<N(accounts), accounts> accounts;

    ///@abi table
    struct global
    {
        uint64_t     ref_rate;        //推荐分割比例（千分之）
        uint64_t     guarantee_rate;  //保障池分割比例（千分之）
        asset        guarantee_pool;  //保障池余额
        asset        bonus_pool;      //分红池余额
        uint64_t     cases_num;       //发起的互助项目总数
        uint64_t     applied_cases;   //申请成功的项目数
        uint64_t     guaranteed_accounts;  //当前受保用户总数
        asset        max_claim;       //单个互助项目可申请的最大token数量
        time         min_apply_interval;   //申请互助的最小时间间隔
        time         time_for_vote;   //投票时间窗口
        time         time_for_observation; //观察期
        time         time_for_announcement;//公示期
        asset        total_key;       //全局key数
        asset        total_skey;      //全局skey数
        asset        tatal_donate;    //已互助总金额

        auto primary_key()const{return 0;}
        EOSLIB_SERIALIZE(global, (ref_rate)(guarantee_rate)(guarantee_pool)(bonus_pool)(cases_num)(applied_cases)(guaranteed_accounts)(max_claim)(min_apply_interval)(time_for_vote)(time_for_observation)(time_for_announcement)(total_key)(total_skey)(tatal_donate))
    };
    eosio::multi_index<N(global), global> global;

    struct aid_entry{
        account_name account;      //互助账号
        asset        aid_quantity; //互助金额
    };

    ///@abi table
    struct cases
    {
        uint64_t        case_id;        //互助项目编号
        checksum256     case_digest;    //项目hash
        account_name    proposer;       //互助申请账号
        asset           required_fund;  //请求的资助金额
        time            start_time;     //开始时间
        time            exec_time;      //互助划款时间
        asset           vote_yes;       //投赞成的SKEY数
    asset           vote_no;        //投反对的SKEY数
        asset           transfer_fund;  //实际划款金额
        vector<aid_entry> aid_list;     //均摊列表

        auto primary_key()const{return case_id;}
        EOSLIB_SERIALIZE(cases, (case_id)(case_digest)(proposer)(required_fund)(start_time)(exec_time)(vote_yes)(vote_no)(transfer_fund)(aid_list))
    };
    eosio::multi_index<N(cases), cases> cases;
};

extern "C"
{
    void apply(uint64_t receiver, uint64_t code, uint64_t action)
    {
        auto self = receiver;
        if( action == N(onerror)) {
            /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */
            eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account");
        }

        medishares thiscontract(self);
        if (code == self || action == N(onerror))
        {   // Action is pushed directly to the contract
            switch (action)
            {
                EOSIO_API(medishares, (init)(transfer)(sellkey)(stakekey)(unstakekey)(propose)(approve)(unapprove)(cancelvote)(execproposal)(delproposal))
            }
        }
        else if (code == N(eosio.token) && action == N(transfer))
        {   //receive message from eosio.token::transfer
            auto transferData = unpack_action_data<transfer_args>();
            if (transferData.to == self && transferData.from != N(eosio.ram) && transferData.from != N(eosio.stake))
            {
                thiscontract.handleTransfer(transferData.from, transferData.to, transferData.quantity, transferData.memo);
            }
        }
        else
        {
            eosio_assert(false, "reject recepient from other contracts");
        }
    }
}

