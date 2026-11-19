// helpers used by every page

function $(selector) {
    return document.querySelector(selector);
}

function $all(selector) {
    return Array.from(document.querySelectorAll(selector));
}

// makes text safe to put inside HTML
function esc(text) {
    return String(text === undefined || text === null ? '' : text)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function rupees(amount) {
    return 'Rs ' + Math.round(amount).toLocaleString('en-IN');
}

// only allow real web links in href attributes
function safeLink(url) {
    return String(url).startsWith('https://') ? url : '#';
}

// "how to apply" text and the buttons that open the official pages in a new tab
function applyHtml(s) {
    let html = '<p><strong>How to apply:</strong> ' + esc(s.how) + '</p><p>';
    if (s.apply) {
        html += '<a class="btn tiny go" href="' + esc(safeLink(s.apply)) + '" target="_blank" rel="noopener">Apply online &#8599;</a> ';
        html += '<a class="btn tiny plain" href="' + esc(safeLink(s.link)) + '" target="_blank" rel="noopener">Official scheme page &#8599;</a>';
    } else {
        html += '<a class="btn tiny go" href="' + esc(safeLink(s.link)) + '" target="_blank" rel="noopener">Official portal / apply &#8599;</a>';
    }
    return html + '</p>';
}

const STATUS_TEXT = {
    eligible: 'Eligible',
    potential: 'Potentially eligible',
    not_eligible: 'Not eligible'
};

// both functions return { ok: true/false, data: <json> }
async function getJson(url) {
    try {
        const response = await fetch(url);
        const data = await response.json();
        return { ok: response.ok, data: data };
    } catch (error) {
        return { ok: false, data: { error: 'Cannot reach the server. Is the program still running?', details: [] } };
    }
}

async function postForm(url, params) {
    try {
        const response = await fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams(params).toString()
        });
        const data = await response.json();
        return { ok: response.ok, data: data };
    } catch (error) {
        return { ok: false, data: { error: 'Cannot reach the server. Is the program still running?', details: [] } };
    }
}
