# Flip 7 strategy evaluator

This C++17 project simulates independent, deterministic solo score-chase
episodes and compares parameterized strategies. It keeps the environment,
strategy, episode loop, experiment runner, and CSV output separate.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run a sweep

```sh
./build/flip7-sweep --strategy all --episodes 10000 --seed 1000000 \
  --target-score 200 --threads 4 --output results.csv
```

The executable accepts `card_count`, `card_sum`, `bust_probability`,
`unique_number`, `hybrid`, or `all`. Each configuration evaluates the same
episode seed range, enabling paired strategy comparisons. The fixed-size
episode batches are distributed among worker threads; a new strategy and
environment are created for every episode.

### Strategy decision rules and sweep parameters

Every strategy chooses between `Hit` and `Finish` from the current public
observation. Its threshold is checked before the next card is drawn. A forced
draw caused by a Flip Three continues independently of the strategy until the
three draws are complete or a round-ending event occurs. When no cards remain
in the deck, strategies finish rather than attempting another hit.

| Strategy (`--strategy`) | Decision rule | Built-in parameter sweep |
| --- | --- | --- |
| `card_count` | Finish when the number of cards in the current round, including special cards, is at least `target_cards`. | 3, 4, or 5 (3 configurations) |
| `card_sum` | Finish when the authoritative current round score (including applicable modifiers and bonuses) is at least `target_score`. | 22 through 31 (10 configurations) |
| `bust_probability` | Finish when estimated duplicate-number bust probability is strictly greater than `max_probability`. | 0.20, 0.21, 0.22, 0.23, or 0.24 (5 configurations) |
| `unique_number` | Finish when the number of distinct numbered cards in the current round is at least `target_unique`. | 3 or 4 (2 configurations) |
| `hybrid` | Finish when **either** the score or distinct-number threshold is met. It does not use bust probability. | Cartesian product: `target_score` 23 through 28 × `target_unique` 3 through 7 (30 configurations) |

The bust-probability strategy needs an accurate tally of cards remaining in
the deck, including cards drawn in previous rounds, which is cumbersome in
real play. It is kept as a simulator strategy, but the built-in sweep is
limited to the strongest thresholds from the ranking. The estimate is the
count of remaining copies of numbers already seen in the hand divided by all
cards remaining; it is zero if a Second Chance is held or the deck is empty,
and the strategy finishes only when the estimate is strictly greater than
`max_probability`.

These ranges describe the executable's built-in sweep; individual strategy
objects validate positive count/score thresholds and a probability threshold
between zero and one. Selecting `all` evaluates 50 parameter configurations.
The ranges were narrowed using the 100,000-episode-per-configuration ranking.
Strong regions were card counts 3-5, card sums 22-31, bust thresholds
0.20-0.24, unique-number targets 3-4, and hybrid score targets 23-28. These
operating ranges are based on that ranking, not a claim that values outside
them can never do better with different rules, seeds, or targets.

### How to follow a strategy as a human

Choose one configuration and keep its rule for every round. For example, to
play `card_count` with `target_cards=4`:

1. Start a round and choose **Hit**.
2. Count every card in your hand, including special cards. A Flip Three card
   makes you draw its three cards before you make another choice; count those
   cards too.
3. If the round has fewer than four cards and no rule has ended it
   automatically, choose **Hit** again.
4. As soon as your hand has four or more cards, choose **Finish** and bank the
   round score.
5. Repeat for the next round until you reach the game's target score.

If a bust, Flip 7, or Freeze ends the round automatically, accept that result
and start the next round if the game is not over. To follow another strategy,
use the same Hit/Finish loop but replace step 3's card-count check with that
strategy's decision rule in the table above.

For `bust_probability`, choose one of the listed `max_probability` thresholds.
Before each voluntary draw, add up the remaining copies of all numbered
values already in your hand, then divide by the total number of cards left in
the deck (including specials). Finish only when this estimate is strictly
greater than your threshold. For example, if 13 duplicate-number cards remain
among 91 deck cards, the estimate is 13/91 = 14.3%; at a 20% threshold hit,
and at a 14% threshold finish. In real multiplayer play, cards revealed by
all players affect the tally, so this strategy requires tracking the shared
deck across turns and rounds.

The main CSV reports mean score and variance, mean rounds/actions/cards,
round-level bust/Flip 7/voluntary-finish rates, target attainment, and the
episode end point. A companion `*_round_distribution.csv` records round
scores, final scores, and round-ending categories.

## Rank and visualize results

```sh
./scripts/rank_results.py results.csv
```

The script writes `results_analysis/ranked_by_mean_rounds.csv` and a
Matplotlib PNG chart showing configurations with the fewest average rounds
per episode. It ranks `mean_rounds` in ascending order and does not use other
metrics to score or break ties. The ranking CSV contains only the rank,
strategy, parameters, episode count, and mean rounds. It uses Python and
Matplotlib; install its dependency with:

```sh
python -m pip install -r requirements.txt
```

Choose how many configurations to display with `--top 15`, or choose another
output folder with `--output-dir path/to/analysis`. It does not modify the
source results.

## Rules and modeling assumptions

The model is deliberately a **single-player score chase**, not a multiplayer
race. It uses one shuffled 94-card deck for the whole episode; the episode
ends when the configured target is reached at a round boundary or when the
deck is exhausted. The deck is not reshuffled between rounds.

The deck contains one 0, `n` copies of each number `n` from 1 through 12,
three each of Second Chance, Freeze, and Flip Three, one each of +2 through
+10 in increments of two, and one x2 card. A round ends on a bust, Flip 7,
Freeze, voluntary finish, or exhaustion during a forced draw. Busting scores
zero for that round. A Second Chance prevents one duplicate-number bust and
is then discarded. In this solo model, Freeze ends the current round and
banks its score; Flip Three forces three additional draws, including any
special-card effects they trigger. Number cards score their value, modifiers
add to the number sum, x2 doubles that subtotal, and Flip 7 adds 15.

These rules and the special-card effects are centralized in the environment;
strategies only receive observations and return Hit/Finish. Observations
expose the remaining-card counts derivable from the public draw history, but
never expose the episode seed or future deck order. An empty round cannot be finished, which
prevents a strategy from creating unbounded zero-score rounds.

Every deck is generated from a per-episode seed with a fixed SplitMix64
Fisher-Yates shuffle. This yields repeatable deck sequences on the same
ruleset version (`flip7-solo-v1`).
