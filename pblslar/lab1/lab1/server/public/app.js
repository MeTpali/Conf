// Клиентская логика: обработка нажатий кнопок формы и асинхронные запросы к серверу.

const form = document.getElementById('convert-form');
const valueInput = document.getElementById('value');
const convertBtn = document.getElementById('convert-btn');
const swapBtn = document.getElementById('swap-btn');
const clearBtn = document.getElementById('clear-btn');
const resultEl = document.getElementById('result');
const historyEl = document.getElementById('history');

// ---------- Работа с API (async/await + Promise) ----------

async function request(url, options) {
  const response = await fetch(url, options);
  const data = await response.json();
  if (!response.ok) {
    throw new Error(data.error || 'Ошибка сервера');
  }
  return data;
}

function convertOnServer(value, from) {
  return request('/api/convert', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ value: value, from: from }),
  });
}

function loadHistory() {
  return request('/api/history');
}

function clearHistory() {
  return request('/api/history', { method: 'DELETE' });
}

// ---------- Отрисовка ----------

function getSelectedUnit() {
  return form.querySelector('input[name="from"]:checked').value;
}

function showResult(record) {
  resultEl.className = 'result ' + (celsiusOf(record) >= 20 ? 'hot' : 'cold');
  resultEl.textContent = record.input + ' °' + record.from + ' = ' + record.output + ' °' + record.to;
}

function showError(message) {
  resultEl.className = 'result error';
  resultEl.textContent = message;
}

function celsiusOf(record) {
  return record.from === 'C' ? record.input : record.output;
}

function renderHistory(items) {
  historyEl.innerHTML = '';
  if (items.length === 0) {
    const li = document.createElement('li');
    li.className = 'empty';
    li.textContent = 'Пока нет преобразований';
    historyEl.appendChild(li);
    return;
  }
  items.forEach(function (item) {
    const li = document.createElement('li');
    const text = document.createElement('span');
    text.textContent = item.input + ' °' + item.from + ' → ' + item.output + ' °' + item.to;
    const time = document.createElement('span');
    time.className = 'time';
    time.textContent = new Date(item.time).toLocaleTimeString('ru-RU');
    li.appendChild(text);
    li.appendChild(time);
    historyEl.appendChild(li);
  });
}

async function refreshHistory() {
  try {
    renderHistory(await loadHistory());
  } catch (e) {
    console.error('Не удалось загрузить историю:', e);
  }
}

// ---------- Обработчики событий ----------

// Кнопка «Преобразовать»
form.addEventListener('submit', async function (event) {
  event.preventDefault();

  const raw = valueInput.value.trim();
  if (raw === '' || isNaN(Number(raw))) {
    valueInput.classList.add('invalid');
    showError('Введите число');
    return;
  }
  valueInput.classList.remove('invalid');

  convertBtn.disabled = true;
  convertBtn.textContent = 'Считаю…';
  try {
    const record = await convertOnServer(Number(raw), getSelectedUnit());
    showResult(record);
    await refreshHistory();
  } catch (e) {
    showError(e.message === 'Failed to fetch' ? 'Нет связи с сервером' : e.message);
  } finally {
    convertBtn.disabled = false;
    convertBtn.textContent = 'Преобразовать';
  }
});

// Кнопка «⇄» — меняет исходную единицу и сразу пересчитывает
swapBtn.addEventListener('click', function () {
  const other = getSelectedUnit() === 'C' ? 'F' : 'C';
  form.querySelector('input[name="from"][value="' + other + '"]').checked = true;
  if (valueInput.value.trim() !== '') {
    form.requestSubmit ? form.requestSubmit() : convertBtn.click();
  }
});

// Кнопка «Очистить» — используем цепочку промисов (then/catch)
clearBtn.addEventListener('click', function () {
  clearHistory()
    .then(function () { return loadHistory(); })
    .then(renderHistory)
    .catch(function (e) { showError(e.message); });
});

valueInput.addEventListener('input', function () {
  valueInput.classList.remove('invalid');
});

// Загружаем историю при открытии страницы (скрипт подключён в конце body, DOM уже готов)
refreshHistory();
