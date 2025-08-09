const routes = {
    "#dashboard": "pages/dashboard.html",
    "#admin": "pages/admin.html",
};

function loadPage() {
    const hash = window.location.hash || "#dashboard";
    const page = routes[hash];

    if (page) {
        fetch(page)
            .then(response => {
                if (!response.ok) {
                    throw new Error(`Failed to load ${page}`);
                }
                return response.text();
            })
            .then(html => {
                document.getElementById("main").innerHTML = html;
            })
            .catch(err => {
                console.error(err);
                document.getElementById("main").innerHTML = `
                    <div class="error">
                        <h1>Error</h1>
                        <p>Failed to load content. Please try again later.</p>
                    </div>`;
            });
    } else {
        // 404
        document.getElementById("content").innerHTML = `
            <div class="error">
                <h1>404 - Page Not Found</h1>
                <p>The page you're looking for does not exist.</p>
            </div>`;
    }
}

window.addEventListener("hashchange", loadPage);
window.addEventListener("DOMContentLoaded", loadPage);