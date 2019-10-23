# token.exchange

## Overview

This project intends to create a smart contract that facilitates an on-chain order book. Users can create markets between two different eosio assets.

## Design

**Deposits:**  
A user can deposit tokens he/she wishes to trade into the exchange contract.  Doing so creates a token balance on the exchange that can be used to place a bid or ask (buy or sell) order.  Exchange balances are protected by the user's account (private key), limiting access of the token balance to only the user and the smart contract itself.

**Withdraw:**  
A user can withdraw any tokens from his/her exchange balance at any time.

**Market Creation:**  
In order to trade assets, a market must first be created.  Markets are created by defining a market pair.  A market pair consists of a **base** and a **quote** denoted as "*base:quote*".  For example EOS:BTC (base = EOS, quote = BTC).

**Place Order:**  
A user can place a bid/ask order to trade one asset for another at a specified price.

A **bid** order wishes to trade the base for the quote (ex: trade EOS for BTC).  The bid, buy price, represents how much of the quote asset is needed to get one unit of the base asset.

An **ask** order wishes to trade the quote for the base (ex: trade BTC for EOS).  The ask, sell price, for the pair represents how much of the quote asset is received for selling one unit of base asset.

Once a trade is submitted, the counter order book (ask if bid, or bid if ask) is immediately checked and determines if a negative spread is available.  If a negative spread is found, the order matching algorithm is triggered and a trade is made.  The order book is updated and the trade results are subtracted/added to each parties exchange balance.

## Continuous Order Matching

Definitions:  
*best bid* = highest price buyer is willing to pay  
*best ask* = lowest price seller willing to offer  
*spread* = difference between best ask and best bid (best ask - best bid)  

Matching Algorithm:

```md
while spread <= 0:
    1. Take the best bid order and best ask order and generate a trade with the following properties:
        volume traded = the minimum volume between both the best bid and best ask orders
        if spread = 0:
            price = best bid price (same as best ask price)
        if spread < 0 (i.e. overlapping in the orderbook):
            price = price of the earlier order submitted
    2. Update the orderbook:
        if best bid volume == best ask volume:
            remove best bid and best ask orders from order book
        else:
            remove the order with the minimum volume (either best bid or best ask) from the orderbook, and update the volume of the other order
```

*example:*  
Base: EOS  
Quote: BTC

Starting Order Book (only Asks)

New Ask: 500 BTC at 831 EOS  
New Ask: 700 BTC at 831 EOS  
New Ask: 500 BTC at 832 EOS  
New Ask: 500 BTC at 835 EOS  

| Time    | Bid/Ask | Price (EOS) | Volume (BTC) |
|---------|---------|-------------|--------------|
| 7:33:01 | Ask     | 831         | 500          |
| 7:33:02 | Ask     | 831         | 700          |
| 7:33:03 | Ask     | 832         | 500          |
| 7:33:04 | Ask     | 835         | 500          |

---

1) Place Bid with no matching Ask orders

New Bid: 300 BTC at price 830 EOS (830 EOS = 1 BTC)

| Time    | Bid/Ask | Price (EOS) | Volume (BTC) |                  |
|---------|---------|-------------|--------------|------------------|
| 7:33:01 | Ask     | 831         | 500          |                  |
| 7:33:02 | Ask     | 831         | 700          |                  |
| 7:33:03 | Ask     | 832         | 500          |                  |
| 7:33:04 | Ask     | 835         | 500          |                  |
| 7:34:00 | Bid     | 830         | 300          | **<- Bid (New)** |

Result: Bid is added to the order book

---

2) Place Bid that partial fills Ask order

New Bid: 100 BTC at price 831 EOS

| Time    | Bid/Ask | Price (EOS) | Volume (BTC) |                       |
|---------|---------|-------------|--------------|-----------------------|
| 7:33:01 | Ask     | 831         | 500          |  **<- Best Ask**      |
| 7:33:02 | Ask     | 831         | 700          |                       |
| 7:33:03 | Ask     | 832         | 500          |                       |
| 7:33:04 | Ask     | 835         | 500          |                       |
| 7:34:00 | Bid     | 830         | 300          |                       |
| 7:34:01 | Bid     | 831         | 100          | **<- Best Bid (New)** |

Result: Buy 100 BTC from the first best Ask order.  Update best Ask order volume and remove the Bid order from book.

| Time    | Bid/Ask | Price (EOS) | Volume (BTC) |
|---------|---------|-------------|--------------|
| 7:33:01 | Ask     | 831         | 400          |
| 7:33:02 | Ask     | 831         | 700          |
| 7:33:03 | Ask     | 832         | 500          |
| 7:33:04 | Ask     | 835         | 500          |
| 7:34:00 | Bid     | 830         | 300          |

---

3) Place Bid that overlaps Ask orders

New Bid: 1000 BTC at price 832 EOS

| Time    | B/Ask | Price (EOS) | Volume (BTC) |                      |
|---------|-------|-------------|--------------|----------------------|
| 7:33:01 | Ask   | 831         | 400          | **<- Best Ask**      |
| 7:33:02 | Ask   | 831         | 700          | **<- Next Best Ask** |
| 7:33:03 | Ask   | 832         | 500          |                      |
| 7:33:04 | Ask   | 835         | 500          |                      |
| 7:34:00 | Bid   | 830         | 300          |                      |
| 7:34:02 | Bid   | 832         | 1000         | **<- Best Bid**      |

Result: Buy entirety of first best Ask order (400 BTC) and 600 BTC of second best Ask order.  Then update the second best Ask order and the remove Bid order from book.

| Time    | Bid/Ask | Price (EOS) | Volume (BTC) |
|---------|---------|-------------|--------------|
| 7:33:02 | Ask     | 831         | 100          |
| 7:33:03 | Ask     | 832         | 500          |
| 7:33:04 | Ask     | 835         | 500          |
| 7:34:00 | Bid     | 830         | 300          |

## Usage

**Contract Deployment:**  
Add the eosio@code permission to the exchange contract account to give the smart contract the authority to sign inline actions.

```bash
cleos set account permission <exchange-account> active --add-code
```

## Actions

**deposit:**  
To make a deposit, a user must transfers tokens from his/her account to the exchange account.

```bash
cleos push action eosio.token transfer '["alice","exchange","5.0000 EOS","eosio.token"]' -p alice@active
```

**withdraw:**  
A user can withdraw his/her exchange balance at any time by calling the withdraw action.

```bash
cleos push action exchange withdraw '{"from":"alice","quantity":{"quantity":"5.0000 EOS","contract":"eosio.token"}}' -p alice@active
```

**createmarket:**  
Creates a trading market between two asset pairs.

- **market_name**: string indicating market pair
- **base**: extened asset defining: the base symbol, percision, and contract account
- **quote**: extened asset defining: the quote symbol, percision, and contract account

```bash
cleos push action exchange createmarket '{"market_name":"EOS/BTC","base":"{"quantity":"1.0000 EOS","contract":"eosio.token"}","quote":"{"quantity":"1.00000000 BTC","contract":"bitcoin"}"}' -p alice@active
```

**trade:**  
A user can place a bid or ask order with their exchange balance.

- **trader**: trader account name
- **market_name**: market pair name
- **order_type**: bid or ask
- **price**: base price
- **volume**: quote volume

bid:

```bash
cleos push action exchange trade '{"trader":"alice","market_name":"EOS/BTC","order_type":"bid","price":"{"quantity":"832.0000 EOS","contract":"eosio.token"}","volume":"{"quantity":"100.00000000 BTC","contract":"bitcoin"}"}' -p alice@active
```

ask:

```bash
cleos push action exchange trade '{"trader":"alice","market_name":"EOS/BTC","order_type":"ask","price":"{"quantity":"832.0000 EOS","contract":"eosio.token"}","volume":"{"quantity":"100.00000000 BTC","contract":"bitcoin"}"}' -p alice@active
```

## Tables

**exaccounts:**  
Scoped to account owner

- **name**: owner of exchange balance
- **balances**: map of extended asset symbol and amount

**markets:**  
Scoped to contract.

Displays all market's and pairs available

- **market_name**: market name
- **base**: base asset in the trading pair
- **quote**: quote asset in the trading pair

**askorders:**  
Scoped to market name (ie. "EOS/BTC")

Ordered from lowest price to highest

- **unique_id**: unique trade id
- **trader**: account making the trade
- **time_stamp**: time stamp of trade
- **price**: base price
- **volume**: quote volume

**bidorders:**  
Scoped to market name (ie. "EOS/BTC")

Ordered from highest price to lowest

- **unique_id**: unique trade id
- **trader**: account making the trade
- **time_stamp**: time stamp of trade
- **price**: base price
- **volume**: quote volume

---

Built with

eosio: v1.8.4

eosio.cdt: version 1.6.3
