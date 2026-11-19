// eligibility check page: form, results, comparison and combinations

let domains = [];
let currentDomain = null;
let lastParams = null;

// questions every domain asks first; the rest are specific to the domain
const BASIC_FIELDS = ['state', 'age', 'gender', 'area', 'category', 'occupation', 'annual_income',
                      'monthly_income', 'household_size', 'disability_pct', 'bpl', 'minority'];

// ---------------- the form ----------------

function fieldHtml(f) {
    let input = '';
    if (f.type === 'number') {
        const hint = (f.unit === 'inr') ? 'amount in rupees' : '';
        input = '<input id="f_' + f.id + '" inputmode="decimal" autocomplete="off" placeholder="' + hint + '">';
    } else if (f.type === 'select') {
        const first = f.required ? '-- Select --' : '-- Select / Not sure --';
        input = '<select id="f_' + f.id + '"><option value="">' + first + '</option>';
        f.options.forEach(function (o) {
            input += '<option value="' + esc(o.value) + '">' + esc(o.label) + '</option>';
        });
        input += '</select>';
    } else {
        input = '<select id="f_' + f.id + '"><option value="">Not sure</option>' +
                '<option value="yes">Yes</option><option value="no">No</option></select>';
    }

    const star = f.required ? ' <span class="required" title="required">*</span>' : '';
    const help = f.help ? '<div class="help">' + esc(f.help) + '</div>' : '';
    return '<div class="field" id="w_' + f.id + '"><label for="f_' + f.id + '">' + esc(f.label) + star +
           '</label>' + input + help + '</div>';
}

function showQuestions(domainId) {
    currentDomain = domains.find(function (d) { return d.id === domainId; });

    $all('#domains label').forEach(function (label) {
        label.classList.toggle('on', label.dataset.id === domainId);
    });

    let basic = '';
    let specific = '';
    currentDomain.fields.forEach(function (f) {
        if (BASIC_FIELDS.includes(f.id)) {
            basic += fieldHtml(f);
        } else {
            specific += fieldHtml(f);
        }
    });

    $('#questions').innerHTML =
        '<fieldset><legend>Step 2a: Personal and socioeconomic details</legend><div class="fields">' + basic + '</div></fieldset>' +
        '<fieldset><legend>Step 2b: Details specific to ' + esc(currentDomain.name) + '</legend>' +
        '<p class="small">Answer what you can. &ldquo;Not sure&rdquo; or blank answers are allowed; the affected schemes will show as ' +
        '<em>potentially eligible</em>. Fields marked <span class="required">*</span> are required.</p>' +
        '<div class="fields">' + specific + '</div></fieldset>';

    $('#prefs').hidden = false;
    $('#results').innerHTML = '';
    $('#errors').innerHTML = '';
    history.replaceState(null, '', '?domain=' + domainId);
}

// reads the typed answers; empty boxes are simply left out
function collectAnswers() {
    const answers = {};
    currentDomain.fields.forEach(function (f) {
        const value = $('#f_' + f.id).value.trim();
        if (value !== '') {
            answers[f.id] = value;
        }
    });
    return answers;
}

function showErrors(data) {
    $all('.field.error').forEach(function (el) { el.classList.remove('error'); });

    const details = data.details || [];
    let html = '<div class="errors" role="alert"><strong>' + esc(data.error) + '</strong>';
    if (details.length > 0) {
        html += '<ul>';
        details.forEach(function (d) { html += '<li>' + esc(d) + '</li>'; });
        html += '</ul>';
    }
    $('#errors').innerHTML = html + '</div>';

    // mark the boxes that have a problem
    currentDomain.fields.forEach(function (f) {
        const bad = details.some(function (d) { return d.startsWith(f.label); });
        if (bad) {
            $('#w_' + f.id).classList.add('error');
        }
    });
    $('#errors').scrollIntoView({ behavior: 'smooth' });
}

async function submitForm(event) {
    event.preventDefault();
    $('#errors').innerHTML = '';

    if (currentDomain === null) {
        $('#errors').innerHTML = '<div class="errors">Please select a domain first.</div>';
        return;
    }

    lastParams = collectAnswers();
    lastParams.domain = currentDomain.id;
    lastParams.objective = $('#objective').value;
    lastParams.horizon = $('#horizon').value;
    lastParams.include_potential = $('#include').value;

    $('#results').innerHTML = '<p class="message">Checking...</p>';
    const result = await postForm('/api/evaluate', lastParams);

    if (!result.ok) {
        $('#results').innerHTML = '';
        showErrors(result.data);
        return;
    }
    showResults(result.data);
    $('#results').scrollIntoView({ behavior: 'smooth' });
}

// ---------------- pieces of the result page ----------------

const SYMBOL = { pass: '&#10004;', fail: '&#10008;', unknown: '?' };

function criteriaHtml(list) {
    let html = '<ul class="criteria">';
    list.forEach(function (c) {
        html += '<li class="' + c.result + '"><span class="sym">' + SYMBOL[c.result] + '</span>' + esc(c.label);
        html += '<span class="why">' + esc(c.detail) + '</span>';
        if (c.children) {
            html += '<ul>';
            c.children.forEach(function (k) {
                html += '<li class="' + k.result + '"><span class="sym">' + SYMBOL[k.result] + '</span>' + esc(k.label) +
                        ' <span class="small">- ' + esc(k.detail) + '</span></li>';
            });
            html += '</ul>';
        }
        html += '</li>';
    });
    return html + '</ul>';
}

// one line describing what the scheme is worth
function valueText(v) {
    const parts = [];
    if (v.annual) {
        parts.push(rupees(v.annual) + ' per year' + (v.years < 99 ? ' for ' + v.years + ' yr' : ''));
    }
    if (v.one_time) parts.push(rupees(v.one_time) + ' one-time');
    if (v.cover) parts.push(rupees(v.cover) + ' insurance cover');
    if (v.future_pension_monthly) parts.push('pension up to ' + rupees(v.future_pension_monthly) + '/month later');
    if (v.in_kind) parts.push(esc(v.in_kind));

    if (parts.length > 0) {
        return parts.join(' &middot; ');
    }
    return v.needs_input ? 'Amount depends on figures you did not enter' : 'No rupee estimate';
}

function schemeCard(s, horizon) {
    const v = s.value;
    let html = '<details class="scheme" data-status="' + s.status + '" id="sch_' + esc(s.id) + '"><summary>';
    html += '<span class="badge ' + s.status + '">' + STATUS_TEXT[s.status] + '</span>';
    html += '<span class="name">' + esc(s.name) + '</span>';
    if (v && (v.money_over_horizon || v.cover)) {
        html += '<span class="small">';
        if (v.money_over_horizon) html += '~' + rupees(v.money_over_horizon) + ' / ' + horizon + ' yr';
        if (v.cover) html += ' cover ' + rupees(v.cover);
        html += '</span>';
    }
    html += '</summary><div class="body">';

    html += '<p><strong>' + esc(s.headline) + '.</strong></p>';
    html += '<p>' + esc(s.description) + '</p>';
    html += '<p><strong>Benefit:</strong> ' + esc(s.benefit_text) + '</p>';
    if (v) {
        html += '<p><strong>Estimated benefit:</strong> ' + valueText(v) + '<br><span class="small">' + esc(v.basis) + '</span></p>';
    }

    html += '<h3>Why: criteria check</h3>';
    html += s.criteria_results.length > 0 ? criteriaHtml(s.criteria_results) : '<p>No specific criteria - open to all residents.</p>';

    if (s.failed.length > 0) {
        html += '<p><strong>Why not eligible:</strong> ' + s.failed.map(esc).join('; ') + '.</p>';
        html += '<p class="small">This reflects only the criteria and sample data in this prototype; state-specific rules may differ.</p>';
    }
    if (s.missing.length > 0) {
        html += '<p><strong>Information still needed to confirm:</strong></p><ul>';
        s.missing.forEach(function (m) { html += '<li>' + esc(m) + '</li>'; });
        html += '</ul>';
    }
    if (s.caveats.length > 0) {
        html += '<p class="small"><strong>Why only &ldquo;potentially eligible&rdquo;:</strong> ' + s.caveats.map(esc).join(' ') + '</p>';
    }
    if (s.documents.length > 0) {
        html += '<p class="small"><strong>Documents usually needed:</strong> ' + s.documents.map(esc).join(', ') + '</p>';
    }
    if (s.notes) html += '<p class="small"><strong>Note:</strong> ' + esc(s.notes) + '</p>';
    if (s.status_note) html += '<p class="small"><strong>Verify:</strong> ' + esc(s.status_note) + '</p>';

    html += applyHtml(s);
    html += '<p class="small">';
    if (s.status !== 'not_eligible') {
        html += '<label><input type="checkbox" class="pick" value="' + esc(s.id) + '"> Select for combination check</label> &middot; ';
    }
    html += 'Dataset checked ' + esc(s.last_checked) + '</p>';
    return html + '</div></details>';
}

function compareTable(schemes, horizon) {
    const rows = schemes.filter(function (s) { return s.value; });
    if (rows.length === 0) {
        return '<p>No schemes to compare.</p>';
    }

    let html = '<div class="table-wrap"><table class="data"><tr><th>Scheme</th><th>Status</th><th>Per year</th>' +
               '<th>One-time</th><th>Cover (insurance)</th><th>Later pension / month</th>' +
               '<th>Est. cash value over ' + horizon + ' yr</th></tr>';
    rows.forEach(function (s) {
        const v = s.value;
        html += '<tr><td><a href="#sch_' + esc(s.id) + '" data-open="' + esc(s.id) + '">' + esc(s.short) + '</a></td>';
        html += '<td><span class="badge ' + s.status + '">' + (s.status === 'eligible' ? 'Eligible' : 'Potential') + '</span></td>';
        html += '<td class="num">' + (v.annual ? rupees(v.annual) : '-') + '</td>';
        html += '<td class="num">' + (v.one_time ? rupees(v.one_time) : '-') + '</td>';
        html += '<td class="num">' + (v.cover ? rupees(v.cover) : '-') + '</td>';
        html += '<td class="num">' + (v.future_pension_monthly ? rupees(v.future_pension_monthly) : '-') + '</td>';
        const total = v.money_over_horizon ? rupees(v.money_over_horizon) : (v.needs_input ? 'enter amount' : '-');
        html += '<td class="num"><strong>' + total + '</strong></td></tr>';
    });
    html += '</table></div>';
    html += '<p class="small">Cash value = one-time + yearly amounts over your planning period. Insurance cover and future pension ' +
            'are shown separately and are not added to cash. Services with no rupee value show &ldquo;-&rdquo;.</p>';
    return html;
}

function basisPill(basis) {
    return '<span class="pill ' + esc(basis) + '">' + esc(basis) + '</span>';
}

function sourceLink(url) {
    return url ? ' <a href="' + esc(safeLink(url)) + '" target="_blank" rel="noopener">source &#8599;</a>' : '';
}

function recommendationHtml(rec, restrictions, horizon) {
    if (!rec.best) {
        return '<p>No eligible or potentially eligible schemes were found, so there is nothing to combine.</p>';
    }

    const best = rec.best;
    const goal = { money: 'maximum money', cover: 'maximum insurance cover', balanced: 'a balance of money and cover' }[rec.objective];
    const lost = rec.naive.money - best.money;

    let html = '<div class="reco"><h3>Recommended valid combination (' + goal + ', ' + horizon + '-year view)</h3><p>';
    best.names.forEach(function (n) { html += '<span class="pill official">' + esc(n) + '</span> '; });
    html += '</p><p><span class="big">' + rupees(best.money) + '</span> estimated cash value';
    if (best.cover) html += ' &middot; <span class="big">' + rupees(best.cover) + '</span> insurance cover';
    if (best.future_pension_monthly) html += ' &middot; later pension up to ' + rupees(best.future_pension_monthly) + '/month';
    html += '</p><p class="small">If every restriction were ignored the total would look like ' + rupees(rec.naive.money);
    if (lost > 0) html += ', but ' + rupees(lost) + ' of that cannot be claimed together';
    html += '.</p></div>';

    if (rec.excluded.length > 0) {
        html += '<h3>Left out of the recommendation</h3><div class="table-wrap"><table class="data"><tr><th>Scheme</th><th>Reason</th></tr>';
        rec.excluded.forEach(function (e) {
            html += '<tr><td>' + esc(e.name) + '</td><td>';
            if (e.because.length === 0) {
                html += 'Not selected under your chosen preference.';
            }
            e.because.forEach(function (b) {
                html += 'Cannot be combined with <strong>' + esc(b.with_name) + '</strong> ' + basisPill(b.basis) +
                        '<br><span class="small">' + esc(b.reason) + '</span><br>';
            });
            html += '</td></tr>';
        });
        html += '</table></div>';
    }

    if (rec.alternatives.length > 0) {
        html += '<h3>Other good valid combinations</h3><ol>';
        rec.alternatives.forEach(function (a) {
            html += '<li>' + a.names.map(esc).join(' + ') + ' &mdash; ' + rupees(a.money);
            if (a.cover) html += ' + ' + rupees(a.cover) + ' cover';
            html += '</li>';
        });
        html += '</ol>';
    }

    html += '<h3>Restrictions between schemes open to you</h3>';
    if (restrictions.length === 0) {
        return html + '<p>No restriction between these schemes is recorded in this dataset. That is not a guarantee; confirm with the authorities.</p>';
    }
    html += '<div class="table-wrap"><table class="data"><tr><th>Scheme A</th><th>Scheme B</th><th>Rule</th><th>Why / source</th></tr>';
    restrictions.forEach(function (r) {
        const rule = (r.type === 'exclusive') ? '<strong>Cannot combine</strong>' : 'Combinable with adjustment';
        html += '<tr><td>' + esc(r.a_name) + '</td><td>' + esc(r.b_name) + '</td><td>' + rule + ' ' + basisPill(r.basis) +
                '</td><td>' + esc(r.reason) + sourceLink(r.source) + '</td></tr>';
    });
    return html + '</table></div>';
}

// ---------------- the whole result page ----------------

function showResults(data) {
    const horizon = data.preferences.horizon;
    const sum = data.summary;
    const open = data.schemes.filter(function (s) { return s.status !== 'not_eligible'; });

    let html = '<h2>Results: ' + esc(data.domain_name) + '</h2>';
    html += '<div class="box warn small">' + esc(data.disclaimer) + '</div>';

    html += '<div class="counts">';
    html += '<div class="count"><b class="green">' + sum.eligible + '</b>Eligible based on the information provided</div>';
    html += '<div class="count"><b class="amber">' + sum.potential + '</b>Potentially eligible; verify with the official authority</div>';
    html += '<div class="count"><b class="red">' + sum.not_eligible + '</b>Not eligible based on the information provided</div>';
    html += '</div>';

    html += '<h3>Scheme-by-scheme result with reasons</h3><div class="filters">';
    html += '<button type="button" class="btn plain tiny on" data-filter="all">All (' + sum.total + ')</button>';
    html += '<button type="button" class="btn plain tiny" data-filter="eligible">Eligible</button>';
    html += '<button type="button" class="btn plain tiny" data-filter="potential">Potential</button>';
    html += '<button type="button" class="btn plain tiny" data-filter="not_eligible">Not eligible</button></div>';
    html += '<div id="cards">' + data.schemes.map(function (s) { return schemeCard(s, horizon); }).join('') + '</div>';

    html += '<h2>Compare benefits</h2>' + compareTable(open, horizon);
    html += '<h2>Combinations and best recommendation</h2>' + recommendationHtml(data.recommendation, data.restrictions, horizon);

    html += '<h3>Check your own combination</h3>';
    html += '<p class="small">Tick &ldquo;Select for combination check&rdquo; inside two or more scheme cards above, then press the button.</p>';
    html += '<p><button type="button" class="btn" id="combine">Check selected schemes together</button> <span class="small" id="picked"></span></p>';
    html += '<div id="combo"></div>';
    html += '<p class="spaced"><button type="button" class="btn plain" id="print">Print / save as PDF</button></p>';

    $('#results').innerHTML = html;
    connectResultButtons();

    const first = $('#cards details');
    if (first) {
        first.open = true;   // open the first card so the explanation is visible at once
    }
}

function connectResultButtons() {
    $all('.filters button').forEach(function (button) {
        button.addEventListener('click', function () {
            $all('.filters button').forEach(function (b) { b.classList.toggle('on', b === button); });
            $all('#cards details').forEach(function (card) {
                card.hidden = button.dataset.filter !== 'all' && card.dataset.status !== button.dataset.filter;
            });
        });
    });

    // clicking a name in the comparison table opens that scheme's card
    $all('a[data-open]').forEach(function (link) {
        link.addEventListener('click', function () {
            const card = $('#sch_' + link.dataset.open);
            if (card) card.open = true;
        });
    });

    $all('.pick').forEach(function (box) {
        box.addEventListener('change', function () {
            $('#picked').textContent = $all('.pick:checked').length + ' selected';
        });
    });

    $('#combine').addEventListener('click', checkCombination);
    $('#print').addEventListener('click', function () { window.print(); });
}

async function checkCombination() {
    const ids = $all('.pick:checked').map(function (box) { return box.value; });
    const params = Object.assign({}, lastParams, { scheme_ids: ids.join(',') });
    const result = await postForm('/api/combine', params);

    if (!result.ok) {
        $('#combo').innerHTML = '<div class="errors">' + esc(result.data.error) + '</div>';
        return;
    }

    const d = result.data;
    const t = d.totals;
    const bad = d.has_conflict || d.has_ineligible;

    let html = '<div class="verdict' + (bad ? ' bad' : '') + '"><strong>' + esc(d.verdict) + '</strong></div>';
    html += '<div class="table-wrap"><table class="data"><tr><th>Scheme A</th><th>Scheme B</th><th>Result</th><th>Details</th></tr>';
    d.pairs.forEach(function (p) {
        let outcome = 'No restriction recorded';
        if (p.status === 'conflict') outcome = '<strong class="red">Cannot combine</strong>';
        if (p.status === 'note') outcome = '<strong class="amber">Combinable, with adjustment</strong>';
        html += '<tr><td>' + esc(p.a_name) + '</td><td>' + esc(p.b_name) + '</td><td>' + outcome + ' ' +
                (p.basis ? basisPill(p.basis) : '') + '</td><td>' + esc(p.reason) + sourceLink(p.source) + '</td></tr>';
    });
    html += '</table></div>';

    if (d.not_eligible.length > 0) {
        html += '<p class="small">Not open to you based on your answers: ' + d.not_eligible.map(esc).join(', ') + '.</p>';
    }
    html += '<p>Total for these schemes over ' + t.horizon + ' yr: <span class="big">' + rupees(t.money) + '</span>';
    if (t.cover) html += ' + ' + rupees(t.cover) + ' cover';
    if (t.future_pension_monthly) html += ' + pension up to ' + rupees(t.future_pension_monthly) + '/month later';
    if (d.has_conflict) {
        html += '<br><strong class="red">This total is NOT valid because the schemes cannot all be claimed together.</strong>';
    }
    html += '</p><p class="small">' + esc(d.note) + '</p>';
    $('#combo').innerHTML = html;
}

// ---------------- start ----------------

async function start() {
    const result = await getJson('/api/domains');
    if (!result.ok) {
        $('#domains').innerHTML = '<span class="small">' + esc(result.data.error) + '</span>';
        return;
    }
    domains = result.data;

    let html = '';
    domains.forEach(function (d) {
        html += '<label data-id="' + esc(d.id) + '"><input type="radio" name="domain" value="' + esc(d.id) + '">' +
                esc(d.name) + ' <span class="small">(' + d.scheme_count + ' schemes)</span></label>';
    });
    $('#domains').innerHTML = html;

    $all('#domains input').forEach(function (radio) {
        radio.addEventListener('change', function () { showQuestions(radio.value); });
    });

    // the home page links here with ?domain=...
    const wanted = new URLSearchParams(location.search).get('domain');
    if (wanted && domains.some(function (d) { return d.id === wanted; })) {
        $('#domains input[value="' + wanted + '"]').checked = true;
        showQuestions(wanted);
    }
}

$('#form').addEventListener('submit', submitForm);
$('#reset').addEventListener('click', function () {
    setTimeout(function () {
        $('#results').innerHTML = '';
        $('#errors').innerHTML = '';
    }, 0);
});

start();
