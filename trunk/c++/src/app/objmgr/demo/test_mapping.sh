#! /bin/sh

cat <<EOF
Seq-entry ::= set {
  seq-set {
EOF

length=1000
for i in 0 1 2 3 4 5 6 7 8 9; do
    gi="$i"500
    cat <<EOF
    seq {
      id {
        gi $gi
      },
      inst {
        repr delta,
        mol dna,
        ext delta {
          literal {
            length $length
          }
        }
      },
      annot {
        {
          data ftable {
EOF
    for bef in "" 1; do
        for aft in "" 1; do
            for minus in "" 1; do
              for ints in 1 2; do
                if test "$ints" = 1; then
                    from=400
                    to=600
                else
                    from=300
                    fromm=400
                    tom=600
                    to=700
                fi

                label="$gi: "
                if test -n "$bef"; then
                    label="$label+"
                fi
                label="$label$from-$to"
                if test -n "$aft"; then
                    label="$label+"
                fi
                if test -n "$minus"; then
                    label="$label minus"
                fi

                echo "{"
                echo "  data region \"$label\","
                if test -n "$bef$aft"; then
                    echo "  partial TRUE,"
                fi
                if test "$ints" = 1; then
                    echo "  location int {"
                    echo "    from $from,"
                    echo "    to $to,"
                    if test -n "$minus"; then
                        echo "    strand minus,"
                    fi
                    echo "    id gi $gi"
                    if test -n "$bef"; then
                        echo ",   fuzz-from lim lt"
                    fi
                    if test -n "$aft"; then
                        echo ",   fuzz-to lim gt"
                    fi
                    echo "  }"
                elif test -z "$minus"; then
                    echo "  location mix {"
                    echo "   int {"
                    echo "    from $from,"
                    echo "    to $fromm,"
                    echo "    id gi $gi"
                    if test -n "$bef"; then
                        echo ",   fuzz-from lim lt"
                    fi
                    echo "   },"
                    echo "   int {"
                    echo "    from $tom,"
                    echo "    to $to,"
                    echo "    id gi $gi"
                    if test -n "$aft"; then
                        echo ",   fuzz-to lim gt"
                    fi
                    echo "   }"
                    echo "  }"
                else
                    echo "  location mix {"
                    echo "   int {"
                    echo "    from $tom,"
                    echo "    to $to,"
                    echo "    strand minus,"
                    echo "    id gi $gi"
                    if test -n "$aft"; then
                        echo ",   fuzz-to lim gt"
                    fi
                    echo "   },"
                    echo "   int {"
                    echo "    from $from,"
                    echo "    to $fromm,"
                    echo "    strand minus,"
                    echo "    id gi $gi"
                    if test -n "$bef"; then
                        echo ",   fuzz-from lim lt"
                    fi
                    echo "   }"
                    echo "  }"
                fi
                echo "},"
              done
            done
        done
    done
    cat <<EOF
            {
              data region "dummy",
              location null NULL
            }
          }
        }
      }
    },
EOF
done
cat <<EOF
    seq {
      id {
        other {
          accession "MASTER",
          version 1
        },
        gi 1
      },
      inst {
        repr seg,
        mol dna,
        topology circular,
        ext seg {
          int {
            from 500,
            to 999,
            id gi 9500
          },
          int {
            from 0,
            to 999,
            id gi 0500
          },
          int {
            from 0,
            to 999,
            strand minus,
            id gi 1500
          },
          int {
            from 0,
            to 449,
            id gi 2500
          },
          int {
            from 0,
            to 449,
            strand minus,
            id gi 3500
          },
          int {
            from 500,
            to 999,
            id gi 4500
          },
          int {
            from 500,
            to 999,
            strand minus,
            id gi 5500
          },
          int {
            from 450,
            to 549,
            id gi 6500
          },
          int {
            from 450,
            to 549,
            strand minus,
            id gi 7500
          },
          int {
            from 0,
            to 999,
            id gi 8500
          },
          int {
            from 0,
            to 499,
            id gi 9500
          }
        }
      }
    }
  }
}
EOF
