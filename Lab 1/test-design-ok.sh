#! /bin/sh

# UCLA CS 111 Lab 1 - Test that valid syntax is processed correctly.

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
cat >& /etc/passwd | tr a-z A-Z | sort -u || echo sort failed!

3 <> /etc/passwd

a<&b >>c  | d <&e >| f | g<&h >& i

(a || b)<&c>|   d

a <& b | (c && d)  >&e

(a >> b) >> c

(a && b     ) >& c

(a >> b) || ( c >| d)

EOF

cat >test.exp <<'EOF'
# 1
      cat>&/etc/passwd \
    |
      tr a-z A-Z \
    |
      sort -u \
  ||
    echo sort failed!
# 2
  3<>/etc/passwd
# 3
    a<&b>>c \
  |
    d<&e>|f \
  |
    g<&h>&i
# 4
  (
     a \
   ||
     b
  )<&c>|d
# 5
    a<&b \
  |
    (
       c \
     &&
       d
    )>&e
# 6
  (
   a>>b
  )>>c
# 7
  (
     a \
   &&
     b
  )>&c
# 8
    (
     a>>b
    ) \
  ||
    (
     c>|d
    )
EOF

../timetrash -p test.sh >test.out 2>test.err || exit

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
  exit 1
}

) || exit

rm -fr "$tmp"
