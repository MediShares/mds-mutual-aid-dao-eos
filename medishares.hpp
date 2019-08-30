#include <functional>
#include <string>
#include <cmath>

#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/asset.hpp>

#define KEY_SYMBOL S(0,KEY)
#define STAKE_SYMBOL S(0,STKEY)

#define PASS_THRESHOLD 50
#define TIME_WINDOW_FOR_VOTE ((uint64_t)(30*24*3600)*1000)

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
    voters(_self, _self)
    {}

    ///@abi action
    void init(const uint64_t guarantee_rate, const uint64_t ref_rate);

    ///@abi action
    void transfer(account_name from, account_name to, asset quantity, string memo);

    ///@abi action
    void sellkey(account_name account, asset key_quantity);

    ///@abi action
    void stakekey(account_name account, asset key_quantity);

    ///@abi action
    void unstakekey(account_name account, asset key_quantity);

    ///@abi action
    void propose(account_name proposer, name case_name, asset required_fund);

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

    ///@abi table
    struct account {
        asset    balance;          //KEY:可用数KEY数，STKEY:冻结KEY数，EOS:保障余额

        uint64_t primary_key()const { return balance.symbol.name(); }

        //EOSLIB_SERIALIZE(account, (balance));
    };

    typedef eosio::multi_index<N(accounts), account> accounts;

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
    struct voters {
        account_name    voter;          //投票账户
        vector<vote_entry> vote_list;   //投票列表

        uint64_t primary_key()const {return voter;}

        EOSLIB_SERIALIZE(voters, (voter)(vote_list));
    };

    eosio::multi_index<N(voters), voters> voters;

    ///@abi table
    struct global
    {
        uint64_t     ref_rate;        //推荐分割比例（千分之）
        uint64_t     guarantee_rate;  //保障池分割比例（千分之）
        asset        guarantee_pool;  //保障池余额
        asset        bonus_pool;      //分红池余额
        uint64_t     cases_num;       //发起的互助项目总数
	
        auto primary_key()const{return 0;}
        EOSLIB_SERIALIZE(global, (ref_rate)(guarantee_rate)(guarantee_pool)(bonus_pool)(cases_num))
    };
    eosio::multi_index<N(global), global> global;

    ///@abi table
    struct cases
    {
        uint64_t        case_id;        //互助项目编号
        name            case_name;      //项目名
        account_name    proposer;       //互助申请账号
        asset           required_fund;  //请求的资助金额（当赞成的KEY超过PASS_THRESHOLD时，实际获得资助金额=vote_yes/(vote_yes+vote_no)*required_fund）
        time            start_time;     //开始时间
        asset           vote_yes;       //投赞成的STKEY数
	asset           vote_no;        //投反对的STKEY数
	
        auto primary_key()const{return case_id;}
        EOSLIB_SERIALIZE(cases, (case_id)(case_name)(proposer)(required_fund)(start_time)(vote_yes)(vote_no))
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
