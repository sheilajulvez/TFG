const API_URL = 'http://localhost:3000/clock';

const hoursInput = document.getElementById('hours');
const minutesInput = document.getElementById('minutes');
const secondsInput = document.getElementById('seconds');
const updateBtn = document.getElementById('updateBtn');

const hourHand = document.getElementById('hourHand');
const minuteHand = document.getElementById('minuteHand');
const secondHand = document.getElementById('secondHand');

let currentData = {
    hours: 0,
    minutes: 0,
    seconds: 0
};

// Fetch initial data
async function fetchData() {
    try {
        const response = await fetch(API_URL);
        const data = await response.json();
        
        // Update local state if valid
        if (data) {
            currentData = {
                hours: data.hours || 0,
                minutes: data.minutes || 0,
                seconds: data.seconds || 0
            };
            updateUI();
        }
    } catch (error) {
        console.error('Error fetching data:', error);
    }
}

// Update the clock visuals and input fields
function updateUI() {
    // Update inputs
    // Only update inputs if they are not currently focused to avoid annoying user
    if (document.activeElement !== hoursInput) hoursInput.value = currentData.hours;
    if (document.activeElement !== minutesInput) minutesInput.value = currentData.minutes;
    if (document.activeElement !== secondsInput) secondsInput.value = currentData.seconds;

    // Update Analog Clock Hands
    const h = currentData.hours;
    const m = currentData.minutes;
    const s = currentData.seconds;

    // Calculate angles
    // Hours: 360 deg / 12 hours = 30 deg per hour. 
    // Plus 0.5 deg per minute (30 deg / 60 min)
    const hDeg = (h % 12) * 30 + m * 0.5;
    const mDeg = m * 6; // 360 / 60 = 6
    const sDeg = s * 6; // 360 / 60 = 6

    hourHand.style.transform = `translateX(-50%) rotate(${hDeg}deg)`;
    minuteHand.style.transform = `translateX(-50%) rotate(${mDeg}deg)`;
    secondHand.style.transform = `translateX(-50%) rotate(${sDeg}deg)`;
}

// Send data to server
async function sendData() {
    // Get values from inputs
    let h = parseInt(hoursInput.value) || 0;
    let m = parseInt(minutesInput.value) || 0;
    let s = parseInt(secondsInput.value) || 0;

    // Clamp values
    h = Math.max(1, Math.min(12, h));
    m = Math.max(0, Math.min(59, m));
    s = Math.max(0, Math.min(59, s));

    // Update inputs with clamped values
    hoursInput.value = h;
    minutesInput.value = m;
    secondsInput.value = s;

    const newData = {
        hours: h,
        minutes: m,
        seconds: s
    };

    try {
        const response = await fetch(API_URL, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(newData)
        });

        if (response.ok) {
            currentData = newData;
            updateUI();
            console.log('Time updated successfully');
        } else {
            console.error('Failed to update time');
        }
    } catch (error) {
        console.error('Error sending data:', error);
    }
}

// Event Listeners
updateBtn.addEventListener('click', sendData);

// Optional: Auto-update visual clock when inputs change (without sending yet)
// to give immediate feedback
[hoursInput, minutesInput, secondsInput].forEach(input => {
    input.addEventListener('input', () => {
        let h = parseInt(hoursInput.value) || 0;
        let m = parseInt(minutesInput.value) || 0;
        let s = parseInt(secondsInput.value) || 0;

        // Temporary visual update
        currentData = { hours: h, minutes: m, seconds: s };
        updateUI();
    });
});

// Initial load
fetchData();

// Optional: Poll every few seconds to keep in sync if changed elsewhere
setInterval(fetchData, 5000);
