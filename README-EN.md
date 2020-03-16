# Contract illustrate
## Contract Tables

### global
The global table store global info of the contract, has members as following.

member | description 
 ---------|----------
ref_rate  | share bonus rate (‰)
guarantee_rate  | the rate that transfer to mutual pool (‰)
guarantee_pool  | balance of mutual pool
bonus_pool  | balance of governance pool 
cases_num  | the number of total mutual aid incident
applied_cases  | the number of mutual aid incident that pass the vote
guaranteed_accounts  | the number of guaranteed accounts
max_claim  | the maximum application funding for a mutual aid events
min_apply_interval  | the minimum interval between two mutual aid events for one person
time_for_vote  | the period for voting after mutual aid events begin
time_for_observation  | the period for observation after user join mutual assistance program
time_for_announcement  | the period for announcement after the mutual aid event been implementated
total_key  | total number of KEY
total_skey  | total number of SKEY
tatal_donate  | total funding that have been implementated for mutual aid events
rule_hash  | IPFS hash of mutual aid rules

### accounts
the accounts table store account information and their vote events.

 member | description 
 ---------|----------
account | EOS account
join_time | the time when the user join mutual assistance program
latest_apply_time | the latest time when the user application for mutual aid incident
asset_list | the asset list for the user
vote_list | list of mutual aid events that the user have vote for

### cases
the cases table store the information of mutual aid events.

member | description 
 ---------|----------
case_id | unique id for mutual aid event 
case_digest | digest for the description of mutual aid event
proposer | the  account who apply for mutual aid incident
required_fund | applied funding for this event 
start_time | the time when the event been proposed
exec_time | the time when this event been implementation
vote_yes | the number of SKEYs that vote YES for this event 
vote_no | the number of SKEYs that vote NO for this event 
transfer_fund | actual transfer funding for this event 
aid_list | account list that take part in the aid of this event

### keymarket
the keymarket table store parameters of bancor which determine the convert rate between the KEY and EOS.

## Contract actions
### init
The init operation performs contract initialization, defines some basic parameters of the MutualDAO mutual assistance plan, and the function declaration is as follows:

`void init(uint64_t guarantee_rate, uint64_t ref_rate, asset max_claim, time time_for_observation, time time_for_announcement, time min_apply_interval, time time_for_vote, string rule_hash);`

Parameter description:

&emsp;  &emsp;  &emsp;guarantee_rate : the rate that transfer to mutual pool;

&emsp;  &emsp;  &emsp;  &emsp;  &emsp;ref_rate : share bonus rate;

&emsp;  &emsp;  &emsp;  &emsp;  max_claim : the maximum application funding for a mutual aid events;

&emsp;  time_for_observation : the period for observation after user join mutual assistance program;

time_for_announcement : the period for announcement after the mutual aid event been implementated;

&emsp;  &emsp;min_apply_interval : the minimum interval between two mutual aid events for one person;

&emsp;  &emsp;  &emsp;  time_for_vote : the period for voting after mutual aid events begin;

&emsp;  &emsp;  &emsp;  &emsp;  rule_hash : IPFS hash of mutual aid rules.

### transfer
When the KEY needs to be transferred (the guaranteed balance and SKEY cannot be transferred), perform this operation. Function declaration:

  `void transfer(account_name from, account_name to, asset quantity, string memo);`
  
Parameter description:

&emsp;  from : transfer from;

&emsp;  &emsp;to : transfer to;

quantity : transfer quantity;

&emsp;memo : transfer memo.

### sellkey
Perform a sellkey operation to exchange the held KEY into a corresponding amount of EOS tokens through bancor. The function declares:

 `void sellkey(account_name account, asset key_quantity);`
 
Parameter description:
 
&emsp;  account : seller;

key_quantity : quantity to sell.

### stakekey
Perform the stakekey operation to convert the held KEY staking into SKEY. Function declaration:

`void stakekey(account_name account, asset key_quantity);`

Parameter description:

&emsp;  account : KEY owner;

key_quantity : KEY quantity to stake.
 
### unstakekey
The unstakekey operation can be performed to convert the held SKEY into KEY. The function declares:

`void unstakekey(account_name account, asset key_quantity);`

Parameter description:

&emsp;  account : SKEY owner;

key_quantity : SKEY quantity to unstake
 
### propose
Perform this operation to initiate a mutual aid event. The sponsor's guaranteed balance must be greater than 0 and the observation period has expired.

`void propose(account_name proposer, checksum256 case_digest, asset required_fund);`

Parameter description:

&emsp;  &emsp;proposer : the  account who apply for mutual aid incident;

&emsp;  case_digest : digest for the description of mutual aid event;

&emsp;required_fund : applied funding for this event.

### approve
The user holding the SKEY voted for a mutual aid event by performing the approve operation, and the voting weight was the number of SKEYs he held.

`void approve(account_name account, uint64_t case_id);`

Parameter description:

account : voter;

case_id :  id for mutual aid event.

### unapprove
The user holding the SKEY voted against a mutual aid event by performing an unapprove operation, and the voting weight was the number of SKEYs he held. The function declares:

`void unapprove(account_name account, uint64_t case_id);`

Parameter description:

account : voter;

case_id :  id for mutual aid event.

### cancelvote
The user cancels his previous vote for a mutual aid event, the function declares:

`void cancelvote(account_name account, uint64_t case_id);`

Parameter description:

account : voter;

case_id :  id for mutual aid event.

### execproposal
After the voting window period has elapsed, execute the execproposal operation to execute the mutual aid application for payment, and the function declaration:

`void execproposal(account_name account, uint64_t case_id);`

Parameter description:

account : the EOS account who execute this action；

case_id :  id for mutual aid event.

### delproposal
Perform this operation to delete the mutual aid application. If the mutual aid application is successful, it can only be deleted after the publicity period has expired. The function declaration:

`void delproposal(account_name account, uint64_t case_id);`

Parameter description:

account ：the EOS account who execute this action;

case_id :  id for mutual aid event.
