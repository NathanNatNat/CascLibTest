@echo off

echo Deleting old extraEncryptionKeys.csv if present.
del extraEncryptionKeys.csv

echo Fetching latest encryption keys from https://raw.githubusercontent.com/wowdev/TACTKeys/master/WoW.txt
curl -LO "https://raw.githubusercontent.com/wowdev/TACTKeys/master/WoW.txt"

echo Renaming WoW.txt to EncryptionKeys.txt
ren WoW.txt EncryptionKeys.txt


