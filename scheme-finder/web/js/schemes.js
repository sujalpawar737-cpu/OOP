// scheme directory page: list, filter by domain, search

let timer = null;

function schemeHtml(s) {
    let html = '<details class="scheme"><summary>';
    html += '<span class="name">' + esc(s.name) + '</span>';
    html += '<span class="small">' + esc(s.ministry) + '</span></summary>';
    html += '<div class="body">';
    html += '<p>' + esc(s.description) + '</p>';
    html += '<p><strong>Benefit:</strong> ' + esc(s.benefit_text) + '</p>';

    html += '<h3>Criteria used by this system</h3>';
    if (s.criteria.length === 0) {
        html += '<p>Open to all residents.</p>';
    } else {
        html += '<ul>';
        s.criteria.forEach(function (c) { html += '<li>' + esc(c) + '</li>'; });
        html += '</ul>';
    }

    if (s.selection_based) {
        html += '<p class="small">Selection is competitive or quota based.</p>';
    }
    if (s.list_based) {
        html += '<p class="small">Inclusion depends on an official beneficiary list or local verification.</p>';
    }
    if (s.notes) {
        html += '<p class="small"><strong>Note:</strong> ' + esc(s.notes) + '</p>';
    }
    if (s.status_note) {
        html += '<p class="small"><strong>Verify:</strong> ' + esc(s.status_note) + '</p>';
    }
    html += applyHtml(s);
    html += '<p class="small">Dataset checked: ' + esc(s.last_checked) + '</p>';
    html += '</div></details>';
    return html;
}

async function loadList() {
    const domain = $('#domain').value;
    const query = $('#search').value.trim();
    const url = '/api/schemes?domain=' + encodeURIComponent(domain) + '&q=' + encodeURIComponent(query);
    const result = await getJson(url);

    if (!result.ok) {
        $('#list').innerHTML = '<p class="message">' + esc(result.data.error) + '</p>';
        return;
    }
    $('#count').textContent = result.data.length + ' scheme(s) shown';
    $('#list').innerHTML = result.data.map(schemeHtml).join('');
}

async function start() {
    const result = await getJson('/api/domains');
    if (result.ok) {
        result.data.forEach(function (d) {
            const option = document.createElement('option');
            option.value = d.id;
            option.textContent = d.name + ' (' + d.scheme_count + ')';
            $('#domain').appendChild(option);
        });
    }
    loadList();
}

$('#domain').addEventListener('change', loadList);
$('#search').addEventListener('input', function () {
    clearTimeout(timer);
    timer = setTimeout(loadList, 250);   // wait until the person stops typing
});

start();
