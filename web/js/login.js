document.getElementById('loginForm').addEventListener('submit', async function(event) {
    const username = document.getElementById('username');
    const password = document.getElementById('password');

    event.preventDefault();
    username.classList.remove('input-error');
    password.classList.remove('input-error');

    const usr = username.value.trim();
    const pwd = password.value.trim();

    const formData = new URLSearchParams();
    formData.append('usr', usr);
    formData.append('pwd', pwd);

    try {
        const response = await fetch('/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            body: formData
        });

        if (response.redirected) {
            window.location.href = response.url;
        } else {
            const errorText = await response.text();
            username.classList.add('input-error');
            password.classList.add('input-error');
        }
    } catch (error) {
        console.error('Login error:', error);
        username.classList.add('input-error');
        password.classList.add('input-error');
    }
});
