'use strict';

const elements = Object.fromEntries([
  'username', 'password', 'login', 'refresh', 'status', 'albumForm', 'recordId',
  'performer', 'album', 'year', 'genre', 'cancel', 'albums'
].map((id) => [id, document.getElementById(id)]));

async function api(path, options = {}) {
  const response = await fetch('/api/v1' + path, {
    credentials: 'same-origin',
    headers: { Accept: 'application/json', ...(options.body ? { 'Content-Type': 'application/json' } : {}) },
    ...options
  });
  if (!response.ok) {
    const payload = await response.json().catch(() => ({ error: `HTTP ${response.status}` }));
    throw new Error(payload.error || `HTTP ${response.status}`);
  }
  return response.status === 204 ? null : response.json();
}

function setStatus(message, error = false) {
  elements.status.textContent = message;
  elements.status.style.color = error ? '#fda4af' : '#7dd3fc';
}

function clearForm() {
  elements.albumForm.reset();
  elements.recordId.value = '';
}

async function refresh() {
  try {
    const albums = await api('/albums');
    elements.albums.replaceChildren(...albums.map((item) => {
      const card = document.createElement('article');
      card.className = 'card';
      const title = document.createElement('h3');
      title.textContent = item.album;
      const details = document.createElement('p');
      details.textContent = `${item.performer} · ${item.year} · ${item.genre}`;
      const actions = document.createElement('div');
      actions.className = 'actions';
      const edit = document.createElement('button');
      edit.textContent = 'Изменить';
      edit.onclick = () => {
        elements.recordId.value = item.id;
        elements.performer.value = item.performer;
        elements.album.value = item.album;
        elements.year.value = item.year;
        elements.genre.value = item.genre;
        window.scrollTo({ top: elements.albumForm.offsetTop, behavior: 'smooth' });
      };
      const remove = document.createElement('button');
      remove.className = 'secondary';
      remove.textContent = 'Удалить';
      remove.onclick = async () => {
        try { await api(`/albums/${item.id}`, { method: 'DELETE' }); await refresh(); }
        catch (error) { setStatus(error.message, true); }
      };
      actions.append(edit, remove);
      card.append(title, details, actions);
      return card;
    }));
    setStatus(`Получено записей: ${albums.length}`);
  } catch (error) { setStatus(error.message, true); }
}

elements.login.addEventListener('click', async () => {
  try {
    await api('/auth/login', { method: 'POST', body: JSON.stringify({ username: elements.username.value, password: elements.password.value }) });
    setStatus('Вход выполнен');
    await refresh();
  } catch (error) { setStatus(error.message, true); }
});
elements.refresh.addEventListener('click', refresh);
elements.cancel.addEventListener('click', clearForm);
elements.albumForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  const id = elements.recordId.value;
  const body = JSON.stringify({ performer: elements.performer.value, album: elements.album.value, year: Number(elements.year.value), genre: elements.genre.value });
  try {
    await api(id ? `/albums/${id}` : '/albums', { method: id ? 'PUT' : 'POST', body });
    clearForm();
    await refresh();
  } catch (error) { setStatus(error.message, true); }
});
