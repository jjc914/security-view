const streamEndpoints = [
    { url: '/video_raw', label: 'Raw Camera' },
    { url: '/video_annotated', label: 'Annotated Camera' }
];

let currentIndex = 0;

function updateStream() {
    const img = document.getElementById('videoStream');
    const label = document.getElementById('streamLabel');
    
    img.src = streamEndpoints[currentIndex].url;
    label.textContent = streamEndpoints[currentIndex].label;
}

document.getElementById('prevButton').addEventListener('click', () => {
    currentIndex = (currentIndex - 1 + streamEndpoints.length) % streamEndpoints.length;
    updateStream();
});

document.getElementById('nextButton').addEventListener('click', () => {
    currentIndex = (currentIndex + 1) % streamEndpoints.length;
    updateStream();
});

// Initialize first stream on page load
updateStream();
