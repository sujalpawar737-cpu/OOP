// Simple tests for the eligibility, benefit and combination classes.
// Run from the project folder with:  make test
#include <iostream>

#include "Api.h"
#include "Repository.h"

static int failures = 0;

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            std::cout << "  FAILED: " #condition " (line " << __LINE__ << ")\n";      \
            failures++;                                                               \
        }                                                                             \
    } while (0)

typedef std::map<std::string, std::string> Answers;

int main() {
    SchemeRepository repo("data");
    EligibilityEngine engine;
    CombinationEngine combos(repo.conflicts());

    std::cout << "[1] PM-KISAN: income tax payer is excluded, non-payer is eligible\n";
    {
        Answers a;
        a["state"] = "punjab"; a["age"] = "40"; a["land_status"] = "owner";
        a["govt_employee_self"] = "no"; a["monthly_pension_10k"] = "no"; a["high_office"] = "no";

        a["income_tax_payer"] = "yes";
        UserProfile taxpayer = repo.form().buildProfile("agriculture", a);
        CHECK(engine.assess(*repo.find("pm_kisan"), taxpayer, 3).status == Status::NotEligible);

        a["income_tax_payer"] = "no";
        UserProfile other = repo.form().buildProfile("agriculture", a);
        Assessment result = engine.assess(*repo.find("pm_kisan"), other, 3);
        CHECK(result.status == Status::Eligible);
        CHECK(result.value.annual == 6000);
    }

    std::cout << "[2] A missing answer gives 'potential', never 'eligible'\n";
    {
        Answers a;
        a["state"] = "punjab"; a["age"] = "40"; a["land_status"] = "owner";
        UserProfile p = repo.form().buildProfile("agriculture", a);
        CHECK(engine.assess(*repo.find("pm_kisan"), p, 3).status == Status::Potential);
    }

    std::cout << "[3] PMEGP subsidy depends on category and area\n";
    {
        Answers a;
        a["state"] = "gujarat"; a["age"] = "30"; a["gender"] = "male"; a["area"] = "urban";
        a["category"] = "general"; a["disability_pct"] = "0"; a["minority"] = "no";
        a["business_stage"] = "new"; a["sector"] = "manufacturing"; a["project_cost"] = "1000000";
        a["prior_pmegp_subsidy"] = "no"; a["qualification"] = "10_pass";

        UserProfile general = repo.form().buildProfile("employment", a);
        CHECK(engine.assess(*repo.find("pmegp"), general, 3).value.oneTime == 150000);   // 15%

        a["category"] = "sc"; a["area"] = "rural";
        UserProfile rural = repo.form().buildProfile("employment", a);
        CHECK(engine.assess(*repo.find("pmegp"), rural, 3).value.oneTime == 350000);     // 35%

        a["category"] = "general"; a["area"] = "urban"; a["project_cost"] = "6000000";
        UserProfile big = repo.form().buildProfile("employment", a);
        CHECK(engine.assess(*repo.find("pmegp"), big, 3).status == Status::NotEligible); // above Rs 50 lakh
    }

    std::cout << "[4] Interest subsidy calculation\n";
    {
        Answers a;
        a["state"] = "kerala"; a["age"] = "20"; a["study_level"] = "ug";
        a["course_stream"] = "technical_professional"; a["annual_income"] = "300000";
        a["other_scholarship"] = "no"; a["edu_loan_amount"] = "1500000"; a["course_duration_years"] = "4";
        UserProfile p = repo.form().buildProfile("education", a);
        Assessment r = engine.assess(*repo.find("csis"), p, 3);
        CHECK(r.status == Status::Eligible);
        CHECK(r.value.oneTime == 1000000 * 0.09 * 5);   // loan capped at Rs 10 lakh, 4 years + 1 year moratorium
    }

    std::cout << "[5] Scholarships cannot be combined\n";
    {
        Answers a;
        a["state"] = "bihar"; a["age"] = "20"; a["gender"] = "female"; a["category"] = "sc";
        a["annual_income"] = "200000"; a["disability_pct"] = "0"; a["study_level"] = "ug";
        a["course_stream"] = "technical_professional"; a["class12_percentile"] = "90";
        a["hosteller"] = "no"; a["other_scholarship"] = "no";
        UserProfile p = repo.form().buildProfile("education", a);

        std::vector<Assessment> all = engine.assessAll(repo.inDomain("education"), p, 3);
        std::vector<const Assessment*> candidates;
        for (size_t i = 0; i < all.size(); i++) {
            if (all[i].usable(true)) candidates.push_back(&all[i]);
        }
        Preferences pref;
        std::vector<Combination> top = combos.best(candidates, pref, 3);
        CHECK(!top.empty());

        int scholarships = 0;
        for (size_t i = 0; i < top[0].members.size(); i++) {
            std::string id = candidates[top[0].members[i]]->scheme->id();
            if (id == "pms_sc" || id == "csss" || id == "aicte_pragati") scholarships++;
        }
        CHECK(scholarships <= 1);
        CHECK(combos.best(candidates, pref, 3)[0].money == top[0].money);   // same answer every time
        CHECK(repo.conflicts().find("pms_sc", "csss") != nullptr);
        CHECK(repo.conflicts().find("csss", "pms_sc") != nullptr);          // order does not matter
    }

    std::cout << "[6] PM Vishwakarma cannot be combined with PMEGP / Mudra / SVANidhi\n";
    {
        const char* others[] = {"pmegp", "mudra", "pm_svanidhi"};
        for (int i = 0; i < 3; i++) {
            const Conflict* c = repo.conflicts().find("pm_vishwakarma", others[i]);
            CHECK(c != nullptr && c->type == ConflictType::Exclusive && c->basis == "official");
        }
    }

    std::cout << "[7] Input checking\n";
    {
        Answers bad;
        bad["age"] = "500";
        bool caught = false;
        try {
            repo.form().buildProfile("social", bad);
        } catch (const InputError& e) {
            caught = true;
            CHECK(e.problems().size() >= 2);   // state is required and age is too big
        }
        CHECK(caught);

        Answers good;
        good["state"] = "delhi"; good["age"] = "45"; good["annual_income"] = "1,20,000";
        repo.form().buildProfile("social", good);   // must not throw

        Answers nan;
        nan["state"] = "delhi"; nan["age"] = "nan";
        caught = false;
        try {
            repo.form().buildProfile("social", nan);
        } catch (const InputError&) {
            caught = true;
        }
        CHECK(caught);
    }

    std::cout << "[8] Every scheme has a how-to-apply text and an https link\n";
    {
        std::vector<const Scheme*> all = repo.allSchemes();
        CHECK(all.size() == 56);
        for (size_t i = 0; i < all.size(); i++) {
            CHECK(!all[i]->howToApply().empty());
            CHECK(all[i]->link().rfind("https://", 0) == 0);
        }
    }

    std::cout << "[9] Bad data is reported, not ignored\n";
    {
        bool caught = false;
        try {
            SchemeRepository broken("no_such_folder");
        } catch (const DataError&) {
            caught = true;
        }
        CHECK(caught);
    }

    std::cout << (failures == 0 ? "\nAll tests passed\n" : "\nSOME TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
