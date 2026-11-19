// about page: shows the usage log kept by the C++ program

async function showStats() {
    const result = await getJson('/api/stats');
    const box = $('#stats');

    if (!result.ok) {
        box.innerHTML = '<p class="message">' + esc(result.data.error) + '</p>';
        return;
    }

    const stats = result.data;
    let html = '<p>Total eligibility checks done: <strong>' + stats.total + '</strong></p>';

    if (stats.total === 0) {
        box.innerHTML = html + '<p class="small">No check has been done yet.</p>';
        return;
    }

    html += '<div class="table-wrap"><table class="data"><tr><th>Domain</th><th>Checks</th></tr>';
    stats.by_domain.forEach(function (d) {
        html += '<tr><td>' + esc(d.domain) + '</td><td class="num">' + d.checks + '</td></tr>';
    });
    html += '</table></div>';

    html += '<h3>Latest checks</h3><div class="table-wrap"><table class="data">';
    html += '<tr><th>Time</th><th>Domain</th><th>Eligible</th><th>Potential</th><th>Not eligible</th></tr>';
    stats.recent.forEach(function (r) {
        html += '<tr><td>' + esc(r.when) + '</td><td>' + esc(r.domain) + '</td>';
        html += '<td class="num">' + r.eligible + '</td><td class="num">' + r.potential + '</td>';
        html += '<td class="num">' + r.not_eligible + '</td></tr>';
    });
    html += '</table></div>';
    box.innerHTML = html;
}

showStats();
