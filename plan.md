# Flip 7 Strategy-E nothing about strategies. A strategy must know nothing about the environment's internal implementation. An episodevaluation System — Final Implementation Plan

 ## 1\. Implementation objective

 Build a **high-performance, deterministic C++ simulation framework for evaluating Flip 7 playing strategies**.

 The central design principle is:

 > **The game environment is the testbed; the strategy is the system under test (SUT).**

 The environment must know nothing about strategies. A strategy must know nothing about the environment's internal implementation. An episode/orchestrator connects the two.

 The system should support:

 - exact reproduction of games from seeds;
- millions/billions of independent episodes;
- completely pre-generated decks;
- very cheap card draws;
- isolated strategy instances;
- parameter sweeps;
- multiple strategies running concurrently;
- deterministic parallel execution;
- CSV/statistical output;
- adding new strategies without changing the game engine;
- evaluating both individual configurations and large strategy parameter spaces.

---

 # 2\. Flip 7 game mechanics

 The implementation should model the actual Flip 7 mechanics rather than baking assumptions into the strategies.

 ## Cards

 The numbered deck contains cards whose values are:

```
0
1
1
2
2
2
3
3
3
3
...
12
```

 In other words, the number of copies increases with the card value.

 There are also special cards such as:

 - Second Chance
- Freeze
- Flip Three

 The exact card composition and special-card behavior should be represented in the environment's card-definition layer rather than hard-coded into strategies.

 The deck is randomized **once at the beginning of an episode**.

 The resulting deck order is retained for the complete episode.

---

 # 3\. Round mechanics

 The game is fundamentally a repeated decision process.

 At the beginning of a round, the player has no numbered cards.

 The player repeatedly chooses:

```
HIT
FINISH
```

 ### HIT

 The environment draws the next card from the pre-generated deck.

 The card is added to the player's current round.

 If the card is a numbered card whose value has already appeared in the current round, the player busts.

 The round ends immediately on the bust.

 If the player reaches seven distinct numbered cards, the player achieves **Flip 7**, ending the round with the associated bonus.

 ### FINISH

 The player voluntarily ends the current round.

 The current round score is retained.

 The game proceeds to the next round.

 The environment, rather than the strategy, determines all consequences.

---

 # 4\. Round scoring

 The engine should calculate scoring centrally.

 The strategy should never calculate or mutate the authoritative score.

 Conceptually:

```
round score
    =
sum of unique numbered cards
+ applicable bonuses
```

 Special cards modify the state according to their rules.

 The implementation should expose the resulting score through the observation.

 This prevents different strategies from accidentally implementing different versions of Flip 7 scoring.

---

 # 5\. Game state

 The environment owns the complete authoritative state.

 Conceptually:

```
EnvironmentState {
    deck
    deck_position

    current_round
    current_score

    visible_cards
    seen_number_counts

    second_chance_state
    freeze_state
    flip_three_state

    round_finished
    game_finished
    bust
    flip7
}
```

 No strategy has access to this structure directly.

---

 # 6\. Deck implementation

 Performance is important because the simulator may execute hundreds of millions of card transitions.

 The deck should therefore be **pre-generated**.

 Do not perform:

```
random card selection
remove card from container
recalculate probabilities
```

 on every `HIT`.

 Instead:

```
seed
 ↓
construct complete deck
 ↓
shuffle once
 ↓
cards[0], cards[1], cards[2], ...
```

 A draw becomes approximately:

```
card = deck.cards[position++];
```

 This makes card selection essentially an array lookup.

 ## Remaining-card information

 For efficient state tracking, maintain compact counters.

 For example:

```
std::array<uint8_t, NUM_CARD_TYPES> remaining;
```

 rather than repeatedly constructing maps.

 The environment may additionally maintain:

```
std::array<uint8_t, NUM_CARD_TYPES> seen;
```

 for the current round.

 This is considerably cheaper than general-purpose hash maps for the small fixed Flip 7 state space.

---

 # 7\. Deterministic randomness

 Every episode gets its own seed.

 For example:

```
strategy configuration
episode 0 → seed 1000000
episode 1 → seed 1000001
episode 2 → seed 1000002
...
```

 No global RNG.

 No shared RNG.

 No random state shared between worker threads.

 This guarantees:

```
same strategy
same parameters
same seed
        ↓
exactly same game
```

 and allows parallel and sequential runs to be compared.

---

 # 8\. Environment interface

 The environment should expose only a small interface.

```
class Environment {
public:
    explicit Environment(uint64_t seed);

    Observation observe() const;

    Observation step(Command command);

    bool done() const;

    EpisodeResult result() const;
};
```

 Commands:

```
enum class Command {
    Hit,
    Finish
};
```

 The environment therefore implements a clean state-transition system:

```
Observation
     │
     ▼
 Command
     │
     ▼
Environment
     │
     ▼
new Observation
```

---

 # 9\. Player interface

 A strategy is simply a player implementation.

```
class Player {
public:
    virtual ~Player() = default;

    virtual Command act(
        const Observation& observation
    ) = 0;
};
```

 That interface is intentionally tiny.

 A strategy cannot:

 - draw cards itself;
- manipulate the deck;
- alter scores;
- restart rounds;
- access RNG;
- access hidden state;
- communicate with another strategy.

 It receives an observation and returns:

```
HIT
```

 or:

```
FINISH
```

 This makes the strategy the true SUT.

---

 # 10\. Player observation

 The observation should contain **all information legitimately available to the player**.

 For example:

```
struct Observation {
    int round_score;
    int card_count;

    std::array<CardType, MAX_ROUND_CARDS> cards;

    bool busted;
    bool flip7;
    bool round_finished;
    bool game_finished;

    // allowed information for strategy calculations
    CardStatistics card_statistics;
};
```

 The exact observation contract should be explicitly documented.

 Most importantly:

 > The observation must not contain hidden future deck information.

 A strategy must not be able to cheat by inspecting the environment.

---

 # 11\. Strategies

 Strategies should be independent classes/files.

 They should not be separate game implementations.

 The initial strategy family should be parameterized rather than exploding into dozens of classes.

 ### Card-count strategy

 Parameters might include:

```
target number of cards
```

 Decision:

```
if card_count >= target:
    FINISH
else:
    HIT
```

 ### Card-sum strategy

 Parameters:

```
target score
```

 Decision:

```
if score >= target:
    FINISH
else:
    HIT
```

 ### Bust-probability strategy

 Parameters:

```
maximum acceptable bust probability
```

 Decision:

```
if estimated bust probability > threshold:
    FINISH
else:
    HIT
```

 ### Unique-number strategy

 Parameters:

```
target unique-card count
```

 Potentially incorporates the composition of the current hand.

 ### Hybrid strategy

 Combines multiple observable quantities:

```
score
card count
estimated bust probability
remaining-card statistics
```

 with parameters such as:

```
target score
maximum bust probability
target card count
minimum expected gain
```

---

 # 12\. Strategy parameterization

 The simulator should treat a strategy configuration as data.

 For example:

```
card_sum:
    target_sum = 15

card_sum:
    target_sum = 16

card_sum:
    target_sum = 17
```

 or:

```
bust_probability:
    max_bust_probability = 0.05

bust_probability:
    max_bust_probability = 0.10

bust_probability:
    max_bust_probability = 0.15
```

 This allows the experiment system to enumerate the parameter space automatically.

---

 # 13\. Episode orchestration

 An episode is the only layer that connects a player to an environment.

```
EpisodeResult run(
    uint64_t seed,
    Player& player
);
```

 Its logic is:

```
create Environment(seed)

while environment is not finished:

    observation = environment.observe()

    command = player.act(observation)

    environment.step(command)

return environment.result()
```

 Neither participant needs to know that this loop exists.

---

 # 14\. Complete separation of responsibilities

 ### Environment

 Owns:

 - deck;
- RNG initialization;
- cards;
- rules;
- scoring;
- rounds;
- busts;
- Flip 7;
- special cards;
- terminal state.

 ### Player

 Owns:

 - strategy parameters;
- decision logic;
- optional strategy-local statistics.

 ### Episode

 Owns:

 - interaction between player and environment.

 ### Experiment

 Owns:

 - parameter sweeps;
- number of episodes;
- seed allocation;
- parallel execution;
- aggregation;
- CSV output.

 This separation is essential.

---

 # 15\. Parallelization strategy

 Parallelization should operate at the **episode level**, because episodes are independent.

 For a strategy configuration:

```
CardSum(target=20)

episode 0
episode 1
episode 2
episode 3
...
episode N
```

 each episode has its own:

```
Environment
Deck
RNG
Player
state
```

 Therefore:

```
CPU 0 → episode 0
CPU 1 → episode 1
CPU 2 → episode 2
CPU 3 → episode 3
...
```

 with no synchronization during game execution.

 This is the maximum useful coarse-grained parallelism.

---

 # 16\. Parallelize across strategies as well

 Suppose the sweep contains:

```
CardSum(10)
CardSum(11)
...
CardSum(30)

CardCount(3)
CardCount(4)
...
CardCount(10)

BustProbability(.01)
BustProbability(.02)
...
BustProbability(.30)
```

 There are two independent dimensions:

```
strategy configuration
        ×
episode
```

 Therefore the workload can be flattened into independent jobs:

```
Job {
    strategy configuration
    seed range
    episode count
}
```

 The worker pool consumes jobs.

 Example:

```
Worker 0 → CardSum(10), episodes 0–999
Worker 1 → CardSum(10), episodes 1000–1999
Worker 2 → CardSum(11), episodes 0–999
Worker 3 → BustProbability(.05), episodes 0–999
...
```

 This avoids artificial serialization between strategies.

---

 # 17\. Maximum parallelization model

 The theoretical workload is:

```
N strategies
×
M parameter combinations
×
K episodes
```

 giving:

```
N × M × K
```

 independent episode evaluations.

 The implementation should exploit this with a fixed-size worker pool.

 Do **not** create:

```
one thread per episode
```

 because millions of OS threads are obviously inappropriate.

 Instead:

```
worker count ≈ hardware concurrency
```

 and feed independent jobs to the workers.

---

 # 18\. Avoid false sharing and synchronization

 Workers should maintain local aggregation:

```
LocalStats {
    uint64_t episodes;
    uint64_t rounds;
    uint64_t actions;
    uint64_t busts;
    uint64_t flip7s;
    double score_sum;
    ...
};
```

 Workers should not update global atomic counters after every card.

 Instead:

```
episode
episode
episode
episode
 ↓
local aggregate
 ↓
one merge
```

 This dramatically reduces synchronization overhead.

---

 # 19\. Strategy instances must be isolated

 Every job gets its own strategy instance.

 Never do:

```
shared_strategy.act(...)
```

 from multiple threads.

 Instead:

```
worker
 ├── Environment
 └── Player
```

 for every independent execution context.

 If strategies maintain learning/statistical state later, this architecture continues to work.

---

 # 20\. Simulation outputs

 At minimum, every strategy configuration should produce:

```
strategy
parameters
episodes
mean_score
mean_rounds
mean_actions
bust_rate
flip7_rate
finish_rate
```

 The requested **end point** should be explicitly recorded.

 For example:

```
end_point
```

 with categories such as:

```
BUST
FINISH
FLIP7
GAME_END
```

 depending on the exact game-level interpretation being used.

---

 # 21\. CSV output

 Produce machine-readable CSV such as:

```
strategy,target_sum,episodes,mean_score,mean_rounds,mean_actions,bust_rate,flip7_rate,end_point
card_sum,10,1000000,...
card_sum,11,1000000,...
card_sum,12,1000000,...
```

 For distributions, add separate output files rather than making the main CSV excessively wide.

 For example:

```
results.csv
round_distribution.csv
score_distribution.csv
endpoint_distribution.csv
```

---

 # 22\. Evaluation metrics

 The primary measurements should be:

 ### Game-level

 - final score;
- game completion;
- number of rounds;
- number of actions;
- total cards drawn.

 ### Round-level

 - round score;
- bust rate;
- Flip 7 rate;
- voluntary finish rate;
- cards drawn before termination.

 ### Strategy-level

 - parameter values;
- episodes simulated;
- mean score;
- mean rounds;
- score variance;
- round variance;
- bust probability;
- Flip 7 probability.

 The simulator should retain enough information to calculate additional metrics later without changing the game mechanics.

---

 # 23\. Deterministic experiment reproduction

 Every result row should be reproducible from:

```
strategy
parameters
seed range
episode count
game-rule version
```

 Ideally also record:

```
software version
deck version
```

 This is particularly important when comparing strategy changes.

---

 # 24\. Build structure

 The final C++ project should be a single coherent project:

```
flip7/
├── CMakeLists.txt
│
├── include/flip7/
│   ├── env/
│   ├── player/
│   ├── episode/
│   └── experiment/
│
├── src/
│   ├── env/
│   ├── player/
│   ├── episode/
│   ├── experiment/
│   └── app/
│
├── scripts/
│   ├── build.sh
│   └── run_sweep.sh
│
└── build/
```

 There should be **one CMake dependency graph**, avoiding the previous problems caused by separately built `flip7_engine`, `flip7_simulation`, and orchestration libraries whose interfaces drifted apart.

---

 # 25\. Implementation order

 The implementation should proceed in this order.

 ### Phase 1 — Game primitives

 Implement:

```
Card
CardType
Deck
Command
Observation
RoundResult
EpisodeResult
```

 ### Phase 2 — Environment

 Implement:

```
Environment(seed)
observe()
step(Hit)
step(Finish)
done()
result()
```

 Verify that the environment can run entirely without a player.

 ### Phase 3 — Player interface

 Implement:

```
Player
```

 with one trivial deterministic strategy for integration.

 ### Phase 4 — Episode

 Implement:

```
Episode::run(seed, Player&)
```

 ### Phase 5 — Strategies

 Implement:

```
CardCount
CardSum
BustProbability
UniqueNumber
Hybrid
```

 as independent classes.

 ### Phase 6 — Experiment jobs

 Implement:

```
StrategyConfiguration
SimulationJob
SimulationResult
```

 ### Phase 7 — Parallel runner

 Implement the worker pool and independent episode execution.

 ### Phase 8 — Aggregation

 Implement local worker statistics followed by a final merge.

 ### Phase 9 — CSV

 Implement deterministic result serialization.

 ### Phase 10 — Sweep executable

 Implement the command-line application that enumerates the strategy parameter space.

---

 # 26\. Important design constraint

 The most important architectural invariant should be:

```
             ┌──────────────┐
             │   Strategy   │
             └──────┬───────┘
                    │
                 Command
                    │
                    ▼
             ┌──────────────┐
             │ Environment  │
             └──────┬───────┘
                    │
               Observation
                    │
                    └──────────► Strategy
```

 The strategy **never gets a reference to the environment**.

 The environment **never gets a reference to the strategy**.

 The episode/orchestrator is the only thing connecting them.

 That gives the system the desired properties simultaneously:

 - game correctness is independently testable;
- strategies are independently testable;
- strategies cannot cheat;
- strategies can be swapped without recompiling the environment logic;
- parameter sweeps are straightforward;
- episodes are embarrassingly parallel;
- different strategy configurations can execute simultaneously;
- deterministic seeds make results reproducible;
- performance-critical deck operations remain inside the environment;
- the simulation can scale from a handful of experiments to very large Monte Carlo sweeps.

 ## Final objective

 The finished system should effectively be a **Flip 7 experimental laboratory**:

```
               Parameter Space
                     │
                     ▼
             ┌───────────────┐
             │ Job Generator │
             └───────┬───────┘
                     │
             independent jobs
                     │
        ┌────────────┼────────────┐
        ▼            ▼            ▼
     Worker 0     Worker 1     Worker N
        │            │            │
     Player       Player       Player
        │            │            │
     Episode      Episode      Episode
        │            │            │
     Environment  Environment  Environment
        │            │            │
     local stats  local stats  local stats
        └────────────┼────────────┘
                     ▼
                Aggregation
                     │
                     ▼
                   CSV
```

 The optimization target is therefore **not merely making individual card draws fast**. It is maximizing throughput of independent strategy evaluations while preserving exact game semantics and reproducibility. The combination of pre-generated decks, compact state, isolated environments, stateless-or-local strategies, deterministic seeds, and episode-level worker parallelism should provide the core performance characteristics needed for large-scale Flip 7 strategy evaluation.