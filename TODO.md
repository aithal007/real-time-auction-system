# End-to-end testing TODO

- [ ] Document E2E test cases for: register/login, create auction, view auctions, place bids (valid/invalid), winners, notifications, persistence.
- [ ] Build and run the server + multiple clients to simulate real usage.
- [ ] Capture expected outputs for each scenario.
- [ ] Execute concurrency test: multiple bidders queue bids concurrently for same auction.
- [ ] Execute timer test: auction closes after duration, winner notification delivered.
- [ ] Execute negative tests: bad formats, bidding on ended auction, non-existent user.
- [ ] Validate file persistence: auctions.txt / bids.txt / users.txt updated correctly.
- [ ] Summarize results (pass/fail) and identify any observed issues.

