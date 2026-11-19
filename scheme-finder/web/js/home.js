// home page: one tile for every domain

async function showDomains() {
    const result = await getJson('/api/domains');
    const tiles = $('#tiles');

    if (!result.ok) {
        tiles.innerHTML = '<p class="message">' + esc(result.data.error) + '</p>';
        return;
    }

    let html = '';
    result.data.forEach(function (d) {
        html += '<a class="tile" href="check.html?domain=' + esc(d.id) + '">';
        html += '<strong>' + esc(d.name) + '</strong>';
        html += '<span>' + esc(d.description) + '</span><br>';
        html += '<span class="small">' + d.scheme_count + ' schemes in this prototype</span>';
        html += '</a>';
    });
    tiles.innerHTML = html;
}

showDomains();
