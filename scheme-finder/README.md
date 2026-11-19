# Government Scheme Eligibility Finder

OOP course project (C++). A website that helps Indian citizens find government schemes they may be eligible for.
The user picks a domain, enters personal and socioeconomic details, and the system shows

- which schemes the person is eligible, potentially eligible or not eligible for, **with the reason**
- the estimated benefit of each scheme and a comparison table
- which schemes **cannot be combined** (with the reason, source and confidence)
- the total benefit of a valid combination and the **best valid combination** for the person's preference
- **how to apply** and the official government link for every scheme

Results are always worded as "Eligible based on the information provided" or
"Potentially eligible; verify with the official authority". The system never claims guaranteed eligibility.

This is an academic prototype with a small curated dataset (56 schemes, 5 domains), not an official service.

Team: _(add names and roll numbers here)_

## Run it (Ubuntu terminal)

```bash
sudo apt update
sudo apt install g++ make        # only needed once
cd scheme-finder
make                             # builds the program
./scheme_finder                  # starts the server
```

Open **http://localhost:8080** in the browser. Press Ctrl+C in the terminal to stop.

Other commands:

| Command | What it does |
|---|---|
| `make test` | runs the unit tests |
| `make links` | checks that every official link in the data opens (needs internet) |
| `make clean` | removes the build files |
| `./scheme_finder --port 9000` | use another port |

### Windows (VS Code with MinGW g++)

```
g++ -std=c++17 -O2 src/*.cpp -o scheme_finder -lws2_32 -lwsock32
scheme_finder.exe
```
Run it from the project folder so that it can find the `data` and `web` folders.

## Folder structure

```
src/        C++ classes (.h and .cpp) and main.cpp
data/       the dataset as plain text files
web/        the website: 4 pages, 1 stylesheet, 5 small scripts
lib/        httplib.h  (cpp-httplib, MIT licence - the web server library)
tests/      test_engine.cpp
```

### Classes

| File | Classes |
|---|---|
| `Base.h` | `DataError`, `InputError`, `JsonSerializable`, `Entity`, `Registry<T>` |
| `Utils` | `util::` helper functions, `JsonObject` |
| `UserProfile` | answers of one citizen |
| `Form` | `FieldDef`, `DomainDef`, `FormModel` (questions, validation) |
| `Rule` | `Rule` (abstract), `NumericRule`, `SetRule`, `BoolRule`, `CompositeRule` |
| `Benefit` | `BenefitValue`, `Benefit` (abstract), `FixedBenefit`, `ServiceBenefit`, `PerUnitBenefit`, `PercentBenefit`, `InterestBenefit`, `TieredBenefit`, `CompositeBenefit` |
| `Scheme` | `Linkable`, `Verifiable`, `Scheme` |
| `Eligibility` | `Assessment`, `EligibilityEngine` |
| `Combination` | `Conflict`, `ConflictRegistry`, `Preferences`, `Combination`, `CombinationEngine` |
| `Loader` | `Tokenizer`, `RuleFactory`, `BenefitFactory`, `DataLoader` |
| `Repository` | `SchemeRepository` |
| `UsageLog` | binary usage log |
| `Api` | `ApiController` (builds the JSON for the pages) |

## OOP concepts from the syllabus and where they are used

| Unit | Concept | Where |
|---|---|---|
| I | Classes, objects, constructors, destructor | `Scheme`, `FieldDef`, `UserProfile`, `~Scheme()` |
| I | Static member, `this` pointer | `Scheme::liveCount()`; chained setters like `FieldDef::setUnit()` return `*this` |
| I | Friend function, operator overloading | `operator<<` for `Scheme`; `BenefitValue::operator+=` and `operator+` |
| I | Function overloading | `JsonObject::add()` for text, numbers and true/false |
| II | Inheritance, virtual functions, pure virtual | `Rule` and `Benefit` hierarchies, `evaluate()` |
| II | Multilevel inheritance | `Benefit` -> `FixedBenefit` -> `ServiceBenefit` |
| II | Multiple inheritance, virtual base class | `Scheme` : `Linkable`, `Verifiable` (both virtually inherit `Entity`) |
| II | Upcasting, polymorphism | rules and benefits are stored as `unique_ptr<Rule>` / `unique_ptr<Benefit>` and called through the base class |
| III | Class and function templates | `Registry<T>`, `jsonArray<T>` |
| III | STL | `vector`, `map`, `set`, `queue`, `sort`, `stable_sort` |
| III | Namespace, operator overloading | `util::`, `Combination::operator>` used with `std::greater` |
| IV | Exception handling, custom exceptions | `DataError`, `InputError`, `try` / `catch` in `DataLoader`, `ApiController`, `main` |
| IV | File handling | text files read in `DataLoader`; binary records written in `UsageLog` |

## How it works

1. `main.cpp` loads the data folder into a `SchemeRepository` (the data is checked while loading, so a typo stops the start-up with a clear message) and starts the web server.
2. The page sends the answers to `/api/evaluate`. `FormModel` checks them and builds a `UserProfile`.
3. For each scheme `EligibilityEngine` calls `evaluate()` of every `Rule`. Each rule says Pass, Fail or Unknown. A missing answer is Unknown and can only give "potentially eligible".
4. For schemes the person could receive, the `Benefit` object works out the amount. The basis of every calculation is shown.
5. `CombinationEngine` tries every conflict-free group of schemes (recursion over a bitmask) and picks the best one for the chosen preference. Restrictions come from `data/conflicts.txt`, each with a reason, a source and a confidence (`official` or `conservative`).

## The data files

`schemes.txt`, `fields.txt`, `domains.txt` and `conflicts.txt` use the same simple layout: a header line, then `key = value` lines, and a blank line between records. Rules and benefits are short expressions, for example

```
rules =
    in category sc,st "Belongs to a Scheduled Caste or Tribe"
    num annual_income <= 250000 "Family income up to Rs 2,50,000"
benefit =
    fixed annual=6000 years=99 "Rs 6,000 per year"
```

To add a scheme, copy an existing record in `data/schemes.txt`, change it and restart the program. If a field name or value is wrong, the program tells you which scheme and what is wrong.

## Before presenting

- Run `make links` once with internet to check the official links.
- Criteria and amounts of schemes change. The `status_note` lines mark the details most likely to have changed; compare them with the official pages and update `checked =`.
