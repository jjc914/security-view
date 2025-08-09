const preview = document.getElementById('imagePreview');
const wrapper = document.querySelector('.image-wrapper');
const registerButton = document.getElementById('registerFacesButton');

let detectedBoxes = [];
let selectedFaces = new Map();

function resizeImageToMain() {
    const main = document.querySelector('.main');

    if (!preview.complete || preview.naturalWidth === 0) {
        return;
    }

    const mainWidth = main.clientWidth;
    const mainHeight = main.clientHeight;
    const imgRatio = preview.naturalWidth / preview.naturalHeight;
    const mainRatio = mainWidth / mainHeight;

    let displayWidth, displayHeight;

    if (imgRatio > mainRatio) {
        displayWidth = mainWidth;
        displayHeight = mainWidth / imgRatio;
    } else {
        displayHeight = mainHeight;
        displayWidth = mainHeight * imgRatio;
    }

    wrapper.style.width = `${displayWidth}px`;
    wrapper.style.height = `${displayHeight}px`;

    // calculate offsets and scaling for bounding boxes
    const iw = preview.naturalWidth;
    const ih = preview.naturalHeight;
    const ww = wrapper.clientWidth;
    const wh = wrapper.clientHeight;

    const scale = Math.min(ww / iw, wh / ih);
    const offsetX = (ww - displayWidth) / 2;
    const offsetY = (wh - displayHeight) / 2;

    // update bounding boxes and input sizes
    wrapper.querySelectorAll('.face-wrapper').forEach((faceWrapper, index) => {
        const box = detectedBoxes[index];
        if (!box) return;

        const x = offsetX + box.x * scale;
        const y = offsetY + box.y * scale;
        const w = box.width * scale;
        const h = box.height * scale;

        faceWrapper.style.left = `${x}px`;
        faceWrapper.style.top = `${y}px`;
        faceWrapper.style.width = `${w}px`;
        faceWrapper.style.height = `${h}px`;
    });
}

window.addEventListener('resize', resizeImageToMain);

// on image selected
document.getElementById('imageFile').addEventListener('change', function(event) {
    const file = event.target.files[0];
    if (file) {
        preview.src = URL.createObjectURL(file);
        preview.onload = resizeImageToMain;
    } else {
        preview.src = '';
    }
    wrapper.querySelectorAll('.face-wrapper').forEach(el => el.remove());
    console.log("hi");
});

// on detection submit
document.getElementById('detectForm').addEventListener('submit', async function(event) {
    event.preventDefault();

    const imageFile = document.getElementById('imageFile').files[0];
    if (!imageFile) {
        alert('Please select an image.');
        return;
    }

    // request data
    const formData = new FormData();
    formData.append('imageFile', imageFile);

    try {
        const response = await fetch('/detect_faces', {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorText = await response.text();
            return;
        }

        const resultJson = await response.json();

        detectedBoxes = resultJson.boxes;
        selectedFaces.clear();
        await drawBoundingBoxes(detectedBoxes);
    } catch (error) {
        console.error('Error running detection:', error);
    }
});

async function drawBoundingBoxes(boxes) {
    wrapper.querySelectorAll('.face-wrapper').forEach(el => el.remove());

    const faceNames = await fetchFaceNames();

    const iw = preview.naturalWidth;
    const ih = preview.naturalHeight;
    const ww = wrapper.clientWidth;
    const wh = wrapper.clientHeight;

    const scale = Math.min(ww / iw, wh / ih);
    const displayWidth = iw * scale;
    const displayHeight = ih * scale;
    const offsetX = (ww - displayWidth) / 2;
    const offsetY = (wh - displayHeight) / 2;

    boxes.forEach((box, index) => {
        const x = offsetX + box.x * scale;
        const y = offsetY + box.y * scale;
        const w = box.width * scale;
        const h = box.height * scale;

        const faceWrapper = document.createElement('div');
        faceWrapper.style.position = 'absolute';
        faceWrapper.style.width = `${w}px`;
        faceWrapper.style.height = `${h}px`;
        faceWrapper.style.left = `${x}px`;
        faceWrapper.style.top = `${y}px`;
        faceWrapper.classList.add('face-wrapper');

        const dropdownWrapper = document.createElement('div');
        dropdownWrapper.classList.add('dropdown-wrapper');

        const dropdown = document.createElement('select');
        dropdown.className = 'dropdown';
        dropdown.innerHTML = `
            <option value="__new__">New...</option>
            ${faceNames.map(name => `<option value="${name}">${name}</option>`).join('')}
        `;

        dropdownWrapper.appendChild(dropdown);

        const newNameInput = document.createElement('input');
        newNameInput.type = 'text';
        newNameInput.placeholder = 'Enter new name';
        newNameInput.classList.add('new-name-input');

        dropdownWrapper.appendChild(newNameInput);
        faceWrapper.appendChild(dropdownWrapper);

        const guess = (box.guess || "").trim();
        if (guess !== "") {
            const option = Array.from(dropdown.options).find(opt => opt.value.toLowerCase() === guess.toLowerCase());
            if (option) {
                // if tag exists, set
                dropdown.value = option.value;
                selectedFaces.set(box.face_index, option.value);
                newNameInput.style.display = 'none';
            } else {
                // if tag does not exist, treat as "new"
                dropdown.value = "__new__";
                newNameInput.style.display = 'block';
                newNameInput.value = guess;
                selectedFaces.set(box.face_index, guess);
            }
        }

        // create face button
        const faceButton = document.createElement('button');
        faceButton.style.width = '100%';
        faceButton.style.height = '100%';
        faceButton.classList.add('face-button');
        faceWrapper.appendChild(faceButton);

        // on face click
        faceButton.addEventListener('click', (e) => {
            e.preventDefault();
            e.stopPropagation();

            faceButton.classList.toggle('selected');

            if (faceButton.classList.contains('selected')) {
                let value = dropdown.value;
                if (value === "__new__") {
                    value = newNameInput.value.trim();
                }
                if (value !== "") {
                    selectedFaces.set(box.face_index, value);
                }
            } else {
                selectedFaces.delete(box.face_index);
            }
        });

        // on dropdown change
        dropdown.addEventListener('change', (e) => {
            const value = e.target.value;
            if (value === "__new__") {
                newNameInput.style.display = 'block';
                selectedFaces.delete(box.face_index);  // Wait for user to type
            } else {
                newNameInput.style.display = 'none';
                if (value !== "") {
                    selectedFaces.set(box.face_index, value);
                } else {
                    selectedFaces.delete(box.face_index);
                }
            }
        });

        // handle new name input change
        newNameInput.addEventListener('input', (e) => {
            const value = e.target.value.trim();
            if (value !== "") {
                selectedFaces.set(box.face_index, value);
            } else {
                selectedFaces.delete(box.face_index);
            }
        });

        wrapper.appendChild(faceWrapper);
    });
}

// register faces
registerButton.addEventListener('click', async () => {
    const imageFile = document.getElementById('imageFile').files[0];
    if (!imageFile) {
        alert('Please select an image.');
        return;
    }

    if (selectedFaces.size === 0) {
        alert('Please select at least one face and assign a name.');
        return;
    }

    const facesPayload = Array.from(selectedFaces.entries()).map(([face_index, name]) => {
        let cleanName = name.trim().toLowerCase();
        cleanName = cleanName.replace(/\s+/g, ' ');
        return {
            face_index,
            name: cleanName
        };
    });

    const formData = new FormData();
    formData.append('imageFile', imageFile);
    formData.append('facesJSON', JSON.stringify(facesPayload));
    try {
        const response = await fetch('/register_faces', {
            method: 'POST',
            body: formData
        });

        const resultText = await response.text();
        console.log(resultText);
    } catch (error) {
        console.error('Error registering faces:', error);
    }
});

async function fetchFaceNames() {
    try {
        const response = await fetch('/get_faces');
        if (!response.ok) {
            console.error('Error fetching face names:', await response.text());
            return [];
        }
        const result = await response.json();
        return result.faces || [];
    } catch (error) {
        console.error('Error fetching face names:', error);
        return [];
    }
}
