#! /bin/bash
set -eEuo pipefail

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <aktualizr-repo> <output directory> <revoke>"
  exit 1
fi

AKTUALIZR_REPO="$1"
DEST_DIR="$2"
REVOKE=""
if [ "$#" -eq 3 ]; then
  REVOKE="$3"
fi


akrepo() {
    echo "$AKTUALIZR_REPO --path $DEST_DIR $@"
    "$AKTUALIZR_REPO" --path "$DEST_DIR" "$@"
}

mkdir -p "$DEST_DIR"
trap 'rm -rf "$DEST_DIR"' ERR

IMAGES=$(mktemp -d)
trap 'rm -rf "$IMAGES"' exit
PRIMARY_FIRMWARE="$IMAGES/primary.txt"
echo "primary" > "$PRIMARY_FIRMWARE"
SECONDARY_FIRMWARE="$IMAGES/secondary.txt"
echo "secondary" > "$SECONDARY_FIRMWARE"
if [[ "$REVOKE" = "revoke" ]]; then
    echo "REVOKE"
    akrepo --command revokedelegation --dname role-abc
else
    echo "NORMAL"
    akrepo --command generate --expires 2021-07-04T16:33:27Z
    akrepo --command adddelegation --dname delegation-top --dpattern "ab*"
    akrepo --command adddelegation --dname role-abc --dpattern "abc/*" --dparent delegation-top
    akrepo --command adddelegation --dname role-bcd --dpattern "bcd/*" --dparent delegation-top
    akrepo --command adddelegation --dname role-cde --dpattern "cde/*" --dparent role-bcd
    akrepo --command adddelegation --dname role-def --dpattern "def/*" --dparent delegation-top
    akrepo --command image --filename "$PRIMARY_FIRMWARE" --targetname primary.txt
    akrepo --command image --filename "$SECONDARY_FIRMWARE" --targetname "abc/secondary.txt" --dname role-abc

    # extra images for allTargets testing
    akrepo --command image --targetname "abracadabra" --dname delegation-top --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command image --targetname "abc/target0" --dname role-abc --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command image --targetname "abc/target1" --dname role-abc --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command image --targetname "abc/target2" --dname role-abc --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command image --targetname "bcd/target0" --dname role-bcd --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command image --targetname "cde/target0" --dname role-cde --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command image --targetname "cde/target1" --dname role-cde --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command image --targetname "def/target0" --dname role-def --targetsha256 40c1fb5a90ea02744126187dc8372f9a289c59f1af4afd9855fd2285f9648bb3 --targetsha512 671718e0c9025135aba25bca6b794920cee047a8031e1f955d5c4d82072422467af5d367243f4113d1b9ca79321091f738e68f27f136f633a5fc9cd6f430c689 --targetlength 100
    akrepo --command addtarget --hwid primary_hw --serial CA:FE:A6:D2:84:9D --targetname primary.txt
    akrepo --command addtarget --hwid secondary_hw --serial secondary_ecu_serial --targetname "abc/secondary.txt"
    akrepo --command signtargets
fi
