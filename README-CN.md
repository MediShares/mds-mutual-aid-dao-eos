# 合约参数说明
## 合约表及其成员变量

### global表
global表存储项目全局信息，包括如下成员变量：

 成员变量 | 描述 
 ---------|----------
ref_rate  | 推荐分红比例（‰），用户加入互助的金额的对应比例分给推荐人
guarantee_rate  | 互助池分割比例（‰），用户加入互助的金额的对应比例进入互助池
guarantee_pool  | 互助池余额
bonus_pool  | 治理池余额，用户加入互助的金额除去推荐和进入互助池的金额之外，剩下的金额进入治理池，并通过bancor兑换相应数量的KEY
cases_num  | 申请的互助项目总数
applied_cases  | 互助成功项目数
guaranteed_accounts  | 有效保障账户数
max_claim  | 单次最大互助申请金额
min_apply_interval  | 多次申请互助的申请最小时间间隔
time_for_vote  | 投票窗口期长度，投票窗口期内，持有SKEY的用户可以对互助申请进行投票、修改投票和取消投票操作
time_for_observation  | 观察期时间长度，用户加入互助保障后需等观察期过后才能申请互助
time_for_announcement  | 公示期时间长度，申请成功的项目自动进入公示期，公示期内项目无法删除
total_key  | KEY的总数
total_skey  | SKEY的总数
tatal_donate  | 已互助的总金额
rule_hash  | 互助项目规则的IPFS哈希

### accounts表
accounts表存储用户账户信息、资产和投票列表：

 成员变量 | 描述
 ---------|----------
account | 用户的EOS账户
join_time | 加入互助保障的时间
latest_apply_time | 最近申请互助的时间
asset_list | 用户资产列表，包括三种资产：保障余额、KEY余额、SKEY余额（余额为0的资产不显示）
vote_list | 用户投票列表，每个表项对应对一个互助申请的投票，包括互助编号和投票项，投票项为1表示赞成，0表示反对

### cases表
cases表存储申请互助的项目信息：

 成员变量  | 描述
 ---------|----------
case_id | 互助申请编号
case_digest | 互助申请信息的hash摘要
proposer | 互助申请人，保障余额大于0且已过观察期的用户才具有互助申请资格
required_fund | 申请的互助金额
start_time | 互助申请发起时间
exec_time | 互助申请划款时间
vote_yes | 赞成该互助申请的SKEY数
vote_no | 反对该互助申请的SKEY数
transfer_fund | 实际划款金额，实际划款金额小于等于申请金额，取决于社区投票和保障池余额情况
aid_list | 该互助申请的均摊列表，每个表项由一个互助账号与其对该申请的均摊金额组成

### keymarket表
keymarket表存储进入治理池中的金额兑换KEY的bancor参数。

## 合约操作及参数
### init
init操作执行合约初始化，定义MutualDAO互助计划的一些基本参数，函数声明如下：

`void init(uint64_t guarantee_rate, uint64_t ref_rate, asset max_claim, time time_for_observation, time time_for_announcement, time min_apply_interval, time time_for_vote, string rule_hash);`

参数说明：

&emsp;  &emsp;  &emsp;guarantee_rate：互助池分割比例；

&emsp;  &emsp;  &emsp;  &emsp;  &emsp;ref_rate：推荐分红比例；

&emsp;  &emsp;  &emsp;  &emsp;  max_claim：单次最大互助申请金额；

&emsp;  time_for_observation：观察期时间长度；

time_for_announcement：公示期时间长度；

&emsp;  &emsp;min_apply_interval：多次申请互助的申请最小时间间隔；

&emsp;  &emsp;  &emsp;  time_for_vote：投票窗口期长度；

&emsp;  &emsp;  &emsp;  &emsp;  rule_hash：互助项目规则的IPFS哈希

### transfer
需要对KEY进行转账时（保障余额和SKEY无法转账），执行该操作，函数声明：

  `void transfer(account_name from, account_name to, asset quantity, string memo);`
  
参数说明：

&emsp;  from：转出账户；

&emsp;  &emsp;to：转入账户；

quantity：转账数量；

&emsp;memo：转账备注

### sellkey
执行sellkey操作将持有的KEY通过bancor兑换成相应数量的EOS代币，函数声明：

 `void sellkey(account_name account, asset key_quantity);`
 
 参数说明：
 
&emsp;  account：卖出账户；

key_quantity：卖出KEY的数量

### stakekey
执行stakekey操作可将持有的KEY抵押兑换成SKEY，函数声明：

`void stakekey(account_name account, asset key_quantity);`

参数说明：

&emsp;  account：抵押账户；

key_quantity：抵押KEY的数量
 
### unstakekey
执行unstakekey操作可将持有的SKEY解抵押兑换成KEY，函数声明：

`void unstakekey(account_name account, asset key_quantity);`

参数说明：

&emsp;  account：解抵押账户；

key_quantity：解抵押SKEY的数量
 
### propose
执行该操作发起互助申请，发起人的保障余额需大于0且已过观察期，函数声明：

`void propose(account_name proposer, checksum256 case_digest, asset required_fund);`

参数说明：

&emsp;  &emsp;proposer：互助申请发起人；

&emsp;  case_digest：互助申请信息的hash摘要；

&emsp;required_fund：申请的互助金额

### approve
持有SKEY的用户通过执行approve操作对某个互助申请投赞成票，投票权重为其持有的SKEY的数量，函数声明：

`void approve(account_name account, uint64_t case_id);`

参数说明：

account：投票账户；

case_id：互助申请编号

### unapprove
持有SKEY的用户通过执行unapprove操作对某个互助申请投反对票，投票权重为其持有的SKEY的数量，函数声明：

`void unapprove(account_name account, uint64_t case_id);`

参数说明：

account：投票账户；

case_id：互助申请编号

### cancelvote
用户取消其之前对某个互助申请的投票，函数声明：

`void cancelvote(account_name account, uint64_t case_id);`

参数说明：

account：投票账户；

case_id：互助申请编号

### execproposal
在投票窗口期过后，执行execproposal操作执行互助申请划款，函数声明：

`void execproposal(account_name account, uint64_t case_id);`

参数说明：

account：执行账户；

case_id：互助申请编号

### delproposal
执行该操作删除互助申请，若是互助成功的项目，需待公示期过后才能删除，函数声明：

`void delproposal(account_name account, uint64_t case_id);`

参数说明：

account：操作账户；

case_id：互助申请编号
