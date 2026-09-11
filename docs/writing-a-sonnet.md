# Writing a program that is a sonnet

`splc -fsonnet -fpentameter=error` accepts a play only if every speech is fourteen lines
of iambic pentameter rhyming ABAB CDCD EFEF GG. This walks through how
`examples/sonnet.spl` was written, because the constraints look impossible until you
notice three things about the language.

## The three things

**Every neutral noun is worth 1.** `a cat`, `a hat`, `a bough`, `a bee` are all 1, and an
adjective doubles whatever follows it. So the *number* a phrase means is fixed by the
count of adjectives, while the *noun* is a free choice. The noun is also usually the last
word of the phrase, which makes it the word that has to rhyme. You get to pick rhymes
without changing the arithmetic.

**Monosyllables are metrically flexible.** The checker treats one-syllable words as fitting
either a strong or a weak position, and *big*, *cat*, *sum*, *thou*, *art*, *and*, *of*
are all monosyllables. A line built from them scans automatically; only the polysyllables
(*thyself*, *remember*, *recall*, *difference*, *between*) have fixed stress and need
placing. `thyself` is 01, so it wants to sit on syllables 2-3, 4-5, 6-7 or 8-9 of the line
(counting from 1), and it fails on 1-2 or 9-10 unless the line has a feminine ending.

**Sentences run across lines, and there are harmless sentences.** A sentence ends at a full
stop, so a line may end mid-sentence wherever the syllable count says. When you have
printed everything you need but still owe the sonnet six lines, `Thou art a hat.` (a
harmless assignment to a value you will not use), `Remember thee.` (push) and
`Recall thy dog.` (pop, the rest of the sentence is ignored) fill lines without changing
what the program does. `And thou art the sum of thyself and me.` is the classic closing
line: *me* is the speaker's own value, which stays 0 if nobody ever assigns to them.

## The plan

Juliet speaks to Romeo. The program prints `Hi!` and a newline, so Romeo must take the
values 72, 105, 33 and 10 in turn, each followed by `Speak thy mind!`.

Powers of two come from stacked adjectives: `a big big big cat` is 8. Sums of them are
`the sum of X and Y`. 72 is 8 times 9, and `twice` doubles, so
`twice twice twice the sum of a big big big cat and a cat` is 8 times (8 + 1). Because the
nouns are free, that becomes `twice twice twice the sum of an elf and a big big big cat`,
which ends the first line on *elf*, a sound that rhymes with *thyself*.

Then 105 is 72 + 33 = 72 + 1 + 32, so the second sentence is
`Thou art the sum of the sum of thyself and a fig and a big big big big big hand.` (the
inner sum adds the 1, the outer adds the 32). 33 is `the sum of a cat and a big big big big
big dog`, and 10 is `the sum of a big big big bat and a big fly`.

## Laying it out

Write the sentences out, then cut them into ten-syllable lines and look at what each line
ends on. Eleven syllables are allowed if the last is unstressed (a feminine ending), and
since every monosyllable is flexible, an eleventh monosyllable is always fine.

```
 1 A  Thou art twice twice twice the sum of an elf        10   elf
 2 B  and a big big big cat. Speak thy mind! And          10   and
 3 A  thou art the sum of the sum of thyself              10   thyself   (01 on 9-10 needs care, see below)
 4 B  and a fig and a big big big big big hand.           10   hand
 5 C  Speak thy mind! Thou art the sum of a cat           10   cat
 6 D  and a big big big big big dog. Speak thy            10   thy
 7 C  mind! Thou art the sum of a big big big bat         10   bat
 8 D  and a big fly. Speak thy mind! Thou art a sky.      11   sky       (feminine ending)
 9 E  Thou art a hat. Thou art a log. Thou art            10   art
10 F  a hog. Remember thee. Recall thy dog.               10   dog
11 E  Thou art the sum of a cat and a cart.               10   cart
12 F  Thou art a big big frog. Thou art a log.            10   log
13 G  Thou art the sum of a rat and a bee.                10   bee
14 G  And thou art the sum of thyself and me.             10   me
```

Things that happened while getting there:

* Line 2 ends on *And*. A sentence may open with a connective, so `And thou art...`
  is legal, and it lets the line break fall before the verb. *And* rhymes with *hand*.
* Line 3 ends on *thyself*, which is 01 on syllables 9-10: exactly the iambic ending.
  The first draft had `thou art the sum of thyself and a twig`, which put *thyself* on
  syllables 6-7 (fine) but left the value wrong; moving the extra `the sum of` inside
  fixed the arithmetic and happened to end the line on the rhyme.
* Line 6 ends on *thy*, split from *mind!* on the next line. *thy* is a full word and it
  rhymes with *fly* and *sky*, so D is the *-y* sound.
* Line 8 needed a tenth and eleventh syllable after the last `Speak thy mind!`, so it
  assigns a harmless `a sky` (Romeo becomes 1; nothing prints it).
* Line 10: `Remember thee. Recall thy dog.` is push then pop of Romeo's own value, a
  no-op that scans (re-MEM-ber thee re-CALL thy dog) and ends on the F rhyme.
* Line 14 puts *thyself* on 8-9 with a leading *And* to make the count, and closes on *me*.

## Checking as you go

```
$ splc --scan examples/sonnet.spl            # every line with its stress pattern and ok/FAIL
$ splc -fsonnet -fsyntax-only examples/sonnet.spl   # line count and rhyme scheme per speech
$ splc -fsonnet -fpentameter=error examples/sonnet.spl -o hi && ./hi
```

`--scan` prints the pattern the checker chose for each line, so a FAIL tells you which
syllable is in the wrong place. A rhyme failure names both words. When a line will not
scan, the fixes in rough order of cheapness are: swap the noun for another of the same
value; move `Speak thy mind!` or the sentence break to the other line; add `And` at the
start of a sentence; or split a sum as `the sum of the sum of X and Y and Z` to change
where the polysyllables fall.

Romeo's answering sonnet in the same file was written the same way, with *thee* (Juliet's
value, 0) as a filler term and *bough/cow*, *cart/art*, *fly/thy*, *frog/dog*, *bee/tree*,
*cat/rat*, *fig/pig* as its rhymes. Note that `a pig` is -1, which is why it can end the
last line of a sonnet that has already finished printing.
