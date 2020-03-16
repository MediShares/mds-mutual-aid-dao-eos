#include "medishares.hpp"
#include <eosiolib/time.hpp>

using namespace eosio;

asset medishares::keymarket::convert_to_exchange( connector& c, asset in ) {
    real_type R(supply.amount);
    real_type C(c.balance.amount+in.amount);
    real_type F(c.weight/1000.0);
    real_type T(in.amount);
    real_type ONE(1.0);

    real_type E = -R * (ONE - std::pow( ONE + T / C, F) );
    int64_t issued = int64_t(E);

    supply.amount += issued;
    c.balance.amount += in.amount;

    return asset( issued, supply.symbol );
}

asset medishares::keymarket::convert_from_exchange( connector& c, asset in ) {
    eosio_assert( in.symbol== supply.symbol, "unexpected asset symbol input" );

    real_type R(supply.amount - in.amount);
    real_type C(c.balance.amount);
    real_type F(1000.0/c.weight);
    real_type E(in.amount);
    real_type ONE(1.0);

    real_type T = C * (std::pow( ONE + E/R, F) - ONE);
    int64_t out = int64_t(T);

    supply.amount -= in.amount;
    c.balance.amount -= out;

    return asset( out, c.balance.symbol );
}

asset medishares::keymarket::convert( asset from, symbol_type to ) {
    auto sell_symbol  = from.symbol;
    auto ex_symbol    = supply.symbol;
    auto base_symbol  = base.balance.symbol;
    auto quote_symbol = quote.balance.symbol;

    if( sell_symbol != ex_symbol ) {
        if( sell_symbol == base_symbol ) {
            from = convert_to_exchange( base, from );
        } else if( sell_symbol == quote_symbol ) {
            from = convert_to_exchange( quote, from );
        } else {
            eosio_assert( false, "invalid sell" );
        }
    } else {
        if( to == base_symbol ) {
            from = convert_from_exchange( base, from );
        } else if( to == quote_symbol ) {
            from = convert_from_exchange( quote, from );
        } else {
            eosio_assert( false, "invalid conversion" );
        }
    }

    if( to != from.symbol )
        return convert( from, to );

    return from;
}

void medishares::init(uint64_t guarantee_rate, uint64_t ref_rate, asset max_claim, time time_for_observation, time time_for_announcement, time min_apply_interval, time time_for_vote, string rule_hash)
{
    eosio_assert(ref_rate > 0 && guarantee_rate > 0, "must positive rate");
    eosio_assert((ref_rate + guarantee_rate) < 1000, "invalid parameters");
    eosio_assert(max_claim.amount > 0, "max_claim must be positive");
    eosio_assert(max_claim.symbol == TOKEN_SYMBOL, "unsupported symbol");
    eosio_assert(min_apply_interval > 0, "min_apply_interval must be positive");
    eosio_assert( 32 <= rule_hash.size() && rule_hash.size() <= 64, "invalid rule hash");

    require_auth(_self);

    auto itr = keymarket.find(KEYCORE_SYMBOL);
    eosio_assert(itr == keymarket.end(), "key market already created");

    itr = keymarket.emplace(_self, [&](auto& k) {
        k.supply.amount = KEY_INIT_SUPPLY;
        k.supply.symbol = KEYCORE_SYMBOL;
        k.base.balance.amount = 400000000;
        k.base.balance.symbol = KEY_SYMBOL;
        k.quote.balance.amount = 20000000000;
        k.quote.balance.symbol = TOKEN_SYMBOL;
    });

    auto glb = global.begin();
    eosio_assert(glb == global.end(), "global table already created");
    glb = global.emplace(_self, [&](auto& gl){
        gl.ref_rate = ref_rate;
        gl.guarantee_rate = guarantee_rate;
        gl.guarantee_pool = asset(0, TOKEN_SYMBOL);
        gl.bonus_pool = asset(0, TOKEN_SYMBOL);
        gl.cases_num = 0;
        gl.applied_cases = 0;
        gl.guaranteed_accounts = 0;
        gl.max_claim = max_claim;
        gl.min_apply_interval = min_apply_interval;
        gl.time_for_vote = time_for_vote;
        gl.time_for_observation = time_for_observation;
        gl.time_for_announcement = time_for_announcement;
        gl.total_key = asset(0, KEY_SYMBOL);
        gl.total_skey = asset(0, STAKE_SYMBOL);
        gl.tatal_donate = asset(0, TOKEN_SYMBOL);
        gl.rule_hash = rule_hash;
    });
}

void medishares::handleTransfer(const account_name from, const account_name to, const asset& quantity, string memo)
{
    if(from == _self || to != _self){
        return;
    }
    eosio_assert(quantity.symbol == TOKEN_SYMBOL, "unsupported symbol");
    eosio_assert(quantity.amount >= 100, "must greater than 0.01 EMDS");

    require_auth(from);

    account_name participator = from;
    account_name referrer = 0;
    uint64_t ref_amount = 0;

    memo.erase(memo.begin(), find_if(memo.begin(), memo.end(), [](int ch) {
        return !isspace(ch);
    }));
    memo.erase(find_if(memo.rbegin(), memo.rend(), [](int ch) {
        return !isspace(ch);
    }).base(), memo.end());

    //memo:"buyfor":"xxxxxxxxxxxx","ref":"xxxxxxxxxxxx"
    auto separator_pos = memo.find("\"buyfor\":\"");
    if (separator_pos != string::npos) {
        auto end_pos =  memo.find("\"", separator_pos+10);
        eosio_assert(end_pos != string::npos, "parse memo error");
        eosio_assert(end_pos - separator_pos <= 22 && end_pos - separator_pos > 10, "invalid account name");
        string beneficiary_account_str = memo.substr(separator_pos + 10, end_pos - 1);
        participator = string_to_name(beneficiary_account_str.c_str());
        eosio_assert(is_account(participator), "participator account does not exist");
    }

    separator_pos = memo.find("\"ref\":\"");
    if (separator_pos != string::npos) {
        auto end_pos =  memo.find("\"", separator_pos+7);
        eosio_assert(end_pos != string::npos, "parse memo error");
        eosio_assert(end_pos - separator_pos <= 19 && end_pos - separator_pos > 7, "invalid account name");
        string referral_account_str = memo.substr(separator_pos + 7, end_pos - 1);
        referrer = string_to_name(referral_account_str.c_str());
        eosio_assert(is_account(referrer), "referrer account does not exist");
    }

    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");

    if(referrer != 0){
        ref_amount = (uint64_t)(quantity.amount * glb->ref_rate / 1000);
        eosio_assert(ref_amount > 0, "referral asset too small");

        action(
            permission_level{_self, N(active)},
            TOKEN_CONTRACT, N(transfer),
            std::make_tuple(_self, referrer, asset(ref_amount, TOKEN_SYMBOL), std::string("Referral bonuses"))
        ).send();
    }

    uint64_t pool_amount = quantity.amount - ref_amount;
    uint64_t guarantee_amount = (uint64_t)((double)glb->guarantee_rate /(double)(1000 - glb->ref_rate) * pool_amount);
    uint64_t bonus_amount = pool_amount - guarantee_amount;
    eosio_assert(bonus_amount > 0, "bonus amount abnormity");

    if(!has_balance(participator, asset(guarantee_amount, TOKEN_SYMBOL))){
        global.modify(glb, 0, [&](auto& gl){
            gl.guaranteed_accounts += 1;
        });
    }
    add_balance(participator, asset(guarantee_amount, TOKEN_SYMBOL), _self);

    auto key_out = asset(0, KEY_SYMBOL);
    const auto& market = keymarket.get(KEYCORE_SYMBOL, "key market does not exist");
    keymarket.modify( market, 0, [&]( auto& km ) {
        key_out = km.convert( asset(bonus_amount, TOKEN_SYMBOL), KEY_SYMBOL);
    });
    eosio_assert( key_out.amount > 0, "can not get any keys in this price, please increase quantity." );
    add_balance(participator, key_out, _self);

    global.modify(glb, 0, [&](auto& gl){
        gl.guarantee_pool = gl.guarantee_pool + asset(guarantee_amount, TOKEN_SYMBOL);
        gl.bonus_pool = gl.bonus_pool + asset(bonus_amount, TOKEN_SYMBOL);
        gl.total_key = gl.total_key + key_out;
    });
}

void medishares::sellkey(account_name account, asset key_quantity){
    require_auth(account);
    eosio_assert(key_quantity.amount > 0, "quantity cannot be negative");
    eosio_assert(key_quantity.symbol == KEY_SYMBOL, "this asset does not supported");
    const auto& market = keymarket.get(KEYCORE_SYMBOL, "this asset market does not exist");

    asset tokens_out;
    keymarket.modify(market, 0, [&](auto& km){
        tokens_out = km.convert(key_quantity, TOKEN_SYMBOL);
    });
    eosio_assert(tokens_out.amount > 0, "token amount too small to transfer");
    action(
        permission_level{_self, N(active)},
        TOKEN_CONTRACT, N(transfer),
        std::make_tuple(_self, account, tokens_out, std::string("sell "+std::to_string(key_quantity.amount)+" key"))
    ).send();

    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(glb->bonus_pool.amount >= tokens_out.amount, "bancor convert error!");
    global.modify(glb, 0, [&](auto& gl){
        gl.bonus_pool.amount -= tokens_out.amount;
        gl.total_key.amount -= key_quantity.amount;
    });

    sub_balance(account, key_quantity);
    auto accounts_itr = accounts.find(account);
    if(accounts_itr->asset_list.size() == 0){
        accounts.erase(accounts_itr);
    }
}

void medishares::transfer(account_name from, account_name to, asset quantity, string memo)
{
    eosio_assert(from != to, "cannot transfer to self");
    require_auth(from);
    eosio_assert(is_account(to), "to account does not exist");

    require_recipient(from);
    require_recipient(to);

    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must transfer positive quantity");
    eosio_assert(quantity.symbol == KEY_SYMBOL, "this asset is not supported or the symbol precision mismatch");
    eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

    sub_balance(from, quantity);
    add_balance(to, quantity, from);

    auto accounts_itr = accounts.find(from);
    if(accounts_itr->asset_list.size() == 0){
        accounts.erase(accounts_itr);
    }
}

bool medishares::has_balance(account_name owner, asset currency){
    auto accounts_itr = accounts.find(owner);
    if(accounts_itr == accounts.end()){
        return false;
    }

    asset_entry asset_e;
    asset_e.balance = currency;
    auto list_itr = std::find(accounts_itr->asset_list.begin(), accounts_itr->asset_list.end(), asset_e);
    if(list_itr == accounts_itr->asset_list.end()){
        return false;
    } else {
        return true;
    }
}

void medishares::sub_balance(account_name owner, asset value){
    auto accounts_itr = accounts.find(owner);
    eosio_assert(accounts_itr != accounts.end(), "account does not exist in this contract");

    asset_entry asset_e;
    asset_e.balance = value;
    auto list_itr = std::find(accounts_itr->asset_list.begin(), accounts_itr->asset_list.end(), asset_e);
    eosio_assert(list_itr != accounts_itr->asset_list.end(), "account does not have this asset");
    eosio_assert(list_itr->balance.amount >= value.amount, "overdrawn balance");

    if(list_itr->balance.amount == value.amount){
        accounts.modify(accounts_itr, _self, [&](auto& a){
            a.asset_list.erase(list_itr);
        });
    }else{
        asset_e.balance.amount = list_itr->balance.amount - value.amount;
        accounts.modify(accounts_itr, _self, [&](auto& a){
            a.asset_list.erase(list_itr);
            a.asset_list.push_back(asset_e);
        });
    }
}

void medishares::add_balance(account_name owner, asset value, account_name ram_payer)
{
    asset_entry asset_e;
    asset_e.balance = value;
    auto accounts_itr = accounts.find(owner);
    if(accounts_itr == accounts.end()){
        accounts_itr = accounts.emplace(ram_payer, [&](auto& a){
            a.account = owner;
            a.asset_list.push_back(asset_e);
            if(value.symbol == TOKEN_SYMBOL){
                a.join_time = now();
            }
            a.latest_apply_time = 0;
        });
    } else {
        auto list_itr = std::find(accounts_itr->asset_list.begin(), accounts_itr->asset_list.end(), asset_e);
    if(list_itr == accounts_itr->asset_list.end()){
            accounts.modify(accounts_itr, ram_payer, [&](auto& a){
                a.asset_list.push_back(asset_e);
                if(value.symbol == TOKEN_SYMBOL){
                    a.join_time = now();
                }
            });
        } else {
            asset_e.balance.amount = list_itr->balance.amount + value.amount;
            accounts.modify(accounts_itr, ram_payer, [&](auto& a){
                a.asset_list.erase(list_itr);
                a.asset_list.push_back(asset_e);
            });
        }
    }
}

void medishares::stakekey(account_name account, asset key_quantity){
    require_auth(account);
    eosio_assert(key_quantity.amount > 0, "quantity cannot be negative");
    eosio_assert(key_quantity.symbol == KEY_SYMBOL, "this asset is not supported or the symbol precision mismatch");

    sub_balance(account, key_quantity);
    add_balance(account, asset(key_quantity.amount, STAKE_SYMBOL), account);

    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(glb->total_key.amount >= key_quantity.amount, "internal error");
    global.modify(glb, 0, [&](auto& gl){
        gl.total_key.amount -= key_quantity.amount;
        gl.total_skey.amount += key_quantity.amount;
    });

    auto accounts_itr = accounts.find(account);

    //若未投票，直接结束
    if(accounts_itr->vote_list.size() == 0)
        return;

    //遍历所投过的case，更新case票数，若case已过投票窗口期，则不更新；若case已删除，则删除对应投票项
    for(auto list_itr = accounts_itr->vote_list.begin(); list_itr != accounts_itr->vote_list.end(); list_itr ++){
        auto case_itr = cases.find(list_itr->case_id);
        if(case_itr == cases.end()){
            accounts.modify(accounts_itr, account, [&]( auto& v){
                v.vote_list.erase(list_itr);
            });
        }else{
            if(case_itr->start_time + glb->time_for_vote < now()){
                continue;
            }
            if(list_itr->agreed){
                cases.modify(case_itr, account, [&]( auto& c){
                    c.vote_yes += asset(key_quantity.amount, STAKE_SYMBOL);
                });
            }else{
                cases.modify(case_itr, account, [&]( auto& c){
                    c.vote_no += asset(key_quantity.amount, STAKE_SYMBOL);
                });
            }
        }
    }
}

void medishares::unstakekey(account_name account, asset key_quantity){
    require_auth(account);
    eosio_assert(key_quantity.amount > 0, "quantity cannot be negative");
    eosio_assert(key_quantity.symbol == STAKE_SYMBOL, "this asset is not supported or the symbol precision mismatch");

    sub_balance(account, key_quantity);
    add_balance(account, asset(key_quantity.amount, KEY_SYMBOL), account);

    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(glb->total_skey.amount >= key_quantity.amount, "internal error");
    global.modify(glb, 0, [&](auto& gl){
        gl.total_key.amount += key_quantity.amount;
        gl.total_skey.amount -= key_quantity.amount;
    });

    auto accounts_itr = accounts.find(account);

    //若未投票，直接结束
    if(accounts_itr->vote_list.size() == 0)
        return;

    //遍历所投过的case，更新case票数，若case已过投票窗口期，则不更新；若case已删除，则删除对应投票项；若对所有SKEY unstake，则删除投票列表
    for(auto list_itr = accounts_itr->vote_list.begin(); list_itr != accounts_itr->vote_list.end(); list_itr ++){
        auto case_itr = cases.find(list_itr->case_id);
        if(case_itr == cases.end()){
            accounts.modify(accounts_itr, account, [&]( auto& a){
                a.vote_list.erase(list_itr);
            });
        }else{
            if(case_itr->start_time + glb->time_for_vote >= now()){
                if(list_itr->agreed){
                    cases.modify(case_itr, account, [&]( auto& c){
                        c.vote_yes -= asset(key_quantity.amount, STAKE_SYMBOL);
                    });
                }else{
                    cases.modify(case_itr, account, [&]( auto& c){
                        c.vote_no -= asset(key_quantity.amount, STAKE_SYMBOL);
                    });
                }
            }
        }
    }
}

bool my_memcmp( void *s1, void *s2, uint32_t n ){
    unsigned char *c1 = (unsigned char *)s1;
    unsigned char *c2 = (unsigned char *)s2;
    for (uint32_t i = 0; i < n; ++i) {
        if ( c1[i] != c2[i] ) {
            return false;
        }
    }
    return true;
}

void medishares::propose(account_name proposer, checksum256 case_digest, asset required_fund){
    require_auth(proposer);
    eosio_assert(required_fund.amount > 0, "required_fund cannot be negative");
    eosio_assert(required_fund.symbol == TOKEN_SYMBOL, "this asset is not supported or the symbol precision mismatch");

    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(glb->guarantee_pool.amount > 0, "the guarantee pool is empty");
    eosio_assert(required_fund.amount <= glb->max_claim.amount, "required fund can not exceed the max claim fund");
    eosio_assert(required_fund.amount <= glb->guarantee_pool.amount, "can not require more than guarantee pool");
    global.modify(glb, 0, [&](auto& gl){
        gl.cases_num += 1;
    });

    const auto& accounts_itr = accounts.get(proposer, "the user does not exist");
    eosio_assert(has_balance(proposer, asset(0, TOKEN_SYMBOL)), "the user do not have guarantee balance");
    eosio_assert(accounts_itr.join_time + glb->time_for_observation <= now(), "can not propose in observation period");
    eosio_assert(accounts_itr.latest_apply_time + glb->min_apply_interval <= now(), "the interval for apply must bigger then min_apply_interval");

    checksum256 cas_d;
    for(auto cas = cases.begin(); cas != cases.end(); cas ++){
        cas_d = cas->case_digest;
        eosio_assert(!my_memcmp(&cas_d, &case_digest, sizeof(checksum256)), "the case already exist");
    }

    cases.emplace(proposer, [&](auto& c) {
        c.case_id = glb->cases_num;
        c.case_digest = case_digest;
        c.proposer = proposer;
        c.required_fund = required_fund;
        c.start_time = now();
        c.exec_time = 0;
        c.vote_yes = asset(0, STAKE_SYMBOL);
        c.vote_no = asset(0, STAKE_SYMBOL);
        c.transfer_fund = asset(0, TOKEN_SYMBOL);
    });

    accounts.modify(accounts_itr, proposer, [&](auto& a){
        a.latest_apply_time = now();
    });
}

void medishares::approve(account_name account, uint64_t case_id){
    require_auth(account);
    const auto& case_itr = cases.get(case_id, "case does not exist");
    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(case_itr.start_time + glb->time_for_vote >= now(), "out of time for vote");

    eosio_assert(has_balance(account, asset(0, STAKE_SYMBOL)), "no stake balance object found");
    asset_entry asset_e;
    asset_e.balance = asset(0, STAKE_SYMBOL);
    auto accounts_itr = accounts.find(account);
    auto asset_list_itr = std::find(accounts_itr->asset_list.begin(), accounts_itr->asset_list.end(), asset_e);

    vote_entry vote_e;
    vote_e.case_id = case_id;
    vote_e.agreed = 1;
    auto vote_list_itr = std::find(accounts_itr->vote_list.begin(), accounts_itr->vote_list.end(), vote_e);
    if(vote_list_itr != accounts_itr->vote_list.end()){
        eosio_assert(vote_list_itr->agreed != 1, "agreeded before");
        accounts.modify(accounts_itr, account, [&](auto& a){
            a.vote_list.erase(vote_list_itr);
            a.vote_list.push_back(vote_e);
        });
        cases.modify(case_itr, account, [&](auto& c){
            c.vote_yes += asset_list_itr->balance;
            c.vote_no -= asset_list_itr->balance;
        });
    }else{
        accounts.modify(accounts_itr, account, [&](auto& a){
            a.vote_list.push_back(vote_e);
        });
        cases.modify(case_itr, account, [&](auto& c){
            c.vote_yes += asset_list_itr->balance;
        });
    }
}

void medishares::unapprove(account_name account, uint64_t case_id){
    require_auth(account);
    const auto& case_itr = cases.get(case_id, "case does not exist");
    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(case_itr.start_time + glb->time_for_vote >= now(), "out of time for vote");

    eosio_assert(has_balance(account, asset(0, STAKE_SYMBOL)), "no stake balance object found");
    asset_entry asset_e;
    asset_e.balance = asset(0, STAKE_SYMBOL);
    auto accounts_itr = accounts.find(account);
    auto asset_list_itr = std::find(accounts_itr->asset_list.begin(), accounts_itr->asset_list.end(), asset_e);

    vote_entry vote_e;
    vote_e.case_id = case_id;
    vote_e.agreed = 0;
    auto vote_list_itr = std::find(accounts_itr->vote_list.begin(), accounts_itr->vote_list.end(), vote_e);
    if(vote_list_itr != accounts_itr->vote_list.end()){
        eosio_assert(vote_list_itr->agreed != 0, "unagreeded before");
        accounts.modify(accounts_itr, account, [&](auto& a){
            a.vote_list.erase(vote_list_itr);
            a.vote_list.push_back(vote_e);
        });
        cases.modify(case_itr, account, [&](auto& c){
            c.vote_yes -= asset_list_itr->balance;
            c.vote_no += asset_list_itr->balance;
        });
    }else{
        accounts.modify(accounts_itr, account, [&](auto& a){
            a.vote_list.push_back(vote_e);
        });
        cases.modify(case_itr, account, [&](auto& c){
            c.vote_no += asset_list_itr->balance;
        });
    }
}

void medishares::cancelvote(account_name account, uint64_t case_id){
    require_auth(account);
    const auto& case_itr = cases.get(case_id, "case does not exist");
    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(case_itr.start_time + glb->time_for_vote >= now(), "out of time for vote");

    eosio_assert(has_balance(account, asset(0, STAKE_SYMBOL)), "no stake balance object found");
    asset_entry asset_e;
    asset_e.balance = asset(0, STAKE_SYMBOL);
    auto accounts_itr = accounts.find(account);
    auto asset_list_itr = std::find(accounts_itr->asset_list.begin(), accounts_itr->asset_list.end(), asset_e);

    vote_entry vote_e;
    vote_e.case_id = case_id;
    vote_e.agreed = 10;
    auto vote_list_itr = std::find(accounts_itr->vote_list.begin(), accounts_itr->vote_list.end(), vote_e);
    eosio_assert(vote_list_itr != accounts_itr->vote_list.end(), "does not vote this case");
    if(vote_list_itr->agreed == 1){
        cases.modify(case_itr, account, [&](auto& c){
            c.vote_yes -= asset_list_itr->balance;
        });
    }else{
        cases.modify(case_itr, account, [&](auto& c){
            c.vote_no -= asset_list_itr->balance;
        });
    }

    accounts.modify(accounts_itr, account, [&](auto& a){
        a.vote_list.erase(vote_list_itr);
    });
}

string uint64_string(uint64_t input, int p)
{
    string result;
    char c;
    int q=p;
    uint8_t base = 10;
    do
    {
        if(q!=0)
        {
            c = input % base;
            input /= base;
            if (c < 10)
                c += '0';
            else
                c += 'A' - 10;
        }
        else
            c = '.';
        result = c + result;
        q--;
    } while (input);

    if(q >= 0)
    {
        while(q > 0)
        {
            result = '0' + result;
            q--;
        }
        result = "0." + result;
    }
    return result;
}

void medishares::execproposal(account_name account, uint64_t case_id){
    require_auth(account);
    auto case_itr = cases.find(case_id);
    eosio_assert(case_itr != cases.end(), "case does not exist");
    eosio_assert(case_itr->exec_time == 0, "the case completed");
    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(case_itr->start_time + glb->time_for_vote < now(), "voting has not been completed");
    eosio_assert(glb->guarantee_pool.amount > 0, "guarantee pool empty");
    const auto& market = keymarket.get(KEYCORE_SYMBOL, "key market does not exist");
    eosio_assert(case_itr->vote_yes.amount > case_itr->vote_no.amount, "insufficient proportion of yes");

    eosio_assert((glb->total_key.amount + glb->total_skey.amount) >= (case_itr->vote_yes.amount + case_itr->vote_no.amount), "prevent speculation through KEY manipulation");
    auto vote_amount = (uint64_t)((double)case_itr->vote_yes.amount/(double)(glb->total_key.amount + glb->total_skey.amount)*case_itr->required_fund.amount);
    uint64_t user_num = glb->guaranteed_accounts;
    auto single_amount = (uint64_t)((double)vote_amount / (double)user_num);
    eosio_assert(single_amount >= 1, "too little to transfer");

    asset_entry asset_e;
    asset_e.balance = asset(single_amount, TOKEN_SYMBOL);
    aid_entry aid_e;
    uint64_t transfer_amount = 0;
    for(auto accounts_itr = accounts.begin(); accounts_itr != accounts.end(); ){
        if(accounts_itr->join_time > 0){
            aid_e.account = accounts_itr->account;
            auto asset_list_itr = std::find(accounts_itr->asset_list.begin(), accounts_itr->asset_list.end(), asset_e);
            if(asset_list_itr->balance.amount > asset_e.balance.amount){
                transfer_amount += asset_e.balance.amount;
                sub_balance(accounts_itr->account, asset_e.balance);
                aid_e.aid_quantity = asset_e.balance;
            }else{
                transfer_amount += asset_list_itr->balance.amount;
                sub_balance(accounts_itr->account, asset_list_itr->balance);
                aid_e.aid_quantity = asset_list_itr->balance;
                global.modify(glb, 0, [&](auto& gl){
                    gl.guaranteed_accounts -= 1;
                });
                if(accounts_itr->asset_list.size() == 0){
                    accounts.erase(accounts_itr);
                    continue;
                }else{
                    accounts.modify(accounts_itr, account, [&](auto& a){
                        a.join_time = 0;
                    });
                }
            }
            cases.modify(case_itr, account, [&](auto& c){
                c.aid_list.push_back(aid_e);
            });
        }
        accounts_itr ++;
    }
    eosio_assert(transfer_amount <= glb->guarantee_pool.amount, "internal error");

    string memo = "case_id:";
    memo.append(std::to_string(case_itr->case_id));
    memo.append(", vote_yes:");
    memo.append(std::to_string(case_itr->vote_yes.amount));
    memo.append("SKEY, vote_no:");
    memo.append(std::to_string(case_itr->vote_no.amount));
    memo.append("SKEY, KEY supply:");
    memo.append(std::to_string(glb->total_key.amount + glb->total_skey.amount));
    memo.append("KEY, vote funding:");
    memo.append(uint64_string(vote_amount, 4));
    memo.append("EMDS, interdependent user:");
    memo.append(std::to_string(user_num));
    memo.append(", each contribute:");
    memo.append(uint64_string(single_amount, 4));
    memo.append("EMDS, actual funding:");
    memo.append(uint64_string(transfer_amount, 4));
    memo.append("EMDS");
    action(
        permission_level{_self, N(active)},
        TOKEN_CONTRACT, N(transfer),
        std::make_tuple(_self, case_itr->proposer, asset(transfer_amount, TOKEN_SYMBOL), memo)
    ).send();

    global.modify(glb, 0, [&](auto& gl){
        gl.guarantee_pool.amount -= transfer_amount;
        gl.applied_cases += 1;
        gl.tatal_donate.amount += transfer_amount;
    });

    cases.modify(case_itr, account, [&]( auto& c){
        c.exec_time = now();
        c.transfer_fund = asset(transfer_amount, TOKEN_SYMBOL);
    });
}

void medishares::delproposal(account_name account, uint64_t case_id){
    require_auth(account);

    auto case_itr = cases.find(case_id);
    eosio_assert(case_itr != cases.end(), "case does not exist");
    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");

    if(case_itr->exec_time == 0){
        if(case_itr->proposer != account){
            eosio_assert(case_itr->start_time + glb->time_for_vote < now(), "voting has not been completed");
            eosio_assert(case_itr->vote_yes.amount <= case_itr->vote_no.amount, "passed cases can not be deleted by others");
        }
    }else{
        eosio_assert(case_itr->exec_time + glb->time_for_announcement >= now(), "can not delete during announcemention");
    }

    cases.erase(case_itr);
}

void medishares::updaterule(string rule_hash){
    require_auth(_self);
    auto glb = global.begin();
    eosio_assert(glb != global.end(), "the global table does not exist");
    eosio_assert(32 <= rule_hash.size() && rule_hash.size() <= 64, "invalid rule hash");
    eosio_assert(rule_hash != glb->rule_hash, "same rule with the old version");

    global.modify(glb, 0, [&](auto& gl){
        gl.rule_hash = rule_hash;
    });
}
