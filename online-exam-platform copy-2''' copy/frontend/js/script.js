// Global variables
let questions = [];
let currentQuestion = 0;
let userAnswers = [];
let markedQuestions = new Set();
let timeLeft = 1800; // 30 minutes in seconds
let timerInterval = null;

// Debug function
function debug(message, data) {
    console.log(`[DEBUG] ${message}`, data || '');
}

// Error display function
function showError(message) {
    const errorDiv = document.getElementById('error-message');
    if (errorDiv) {
        errorDiv.textContent = message;
        errorDiv.style.display = 'block';
        setTimeout(() => {
            errorDiv.style.display = 'none';
        }, 5000);
    }
    console.error(message);
}

// Get question status
function getQuestionStatus() {
    if (markedQuestions.has(currentQuestion)) {
        return 'Marked for Review';
    }
    if (userAnswers[currentQuestion] !== -1) {
        return 'Answered';
    }
    return 'Not Answered';
}

// Load questions from server
async function loadQuestions() {
    try {
        debug('Fetching questions from server...');
        const response = await fetch('http://localhost:8080/api/questions');
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        // Get the response text and clean it immediately
        let text = await response.text();
        debug('Received text:', text.substring(0, 100));
        
        if (!text || text.trim() === '') {
            throw new Error('Empty response from server');
        }
        
        // Clean the text once, right after receiving it
        text = cleanText(text);
        
        // Parse the text into lines and filter out empty lines
        const lines = text.split(/\r?\n/).filter(line => {
            const trimmed = line.trim();
            return trimmed !== '' && !trimmed.match(/^[\r\n\s]*$/);
        });
        
        debug(`Found ${lines.length} lines to parse`);
        
        // Parse each line into a question object
        questions = parseQuestions(text);
        
        if (!questions || questions.length === 0) {
            throw new Error('No valid questions found');
        }
        
        debug(`Successfully loaded ${questions.length} questions`);
        userAnswers = new Array(questions.length).fill(-1);
        
        renderQuestion();
        buildQuestionPalette();
        updateButtonStates();
        startTimer();
        
    } catch (error) {
        console.error('Error loading questions:', error);
        showError(`Failed to load questions: ${error.message}`);
    }
}

// Clean text helper function
function cleanText(text) {
    if (!text) return '';
    
    // First, normalize line endings to \n
    text = text.replace(/\r\n/g, '\n').replace(/\r/g, '\n');
    
    // Remove BOM if present
    text = text.replace(/^\uFEFF/, '');
    
    // Split into lines, clean each line, then rejoin
    return text.split('\n')
        .map(line => {
            // Remove any non-printable characters except spaces and valid unicode
            return line.trim()
                .replace(/[^\x20-\x7E\u0080-\uFFFF\s]/g, '')
                .normalize('NFKC');
        })
        .filter(line => line.length > 0)
        .join('\n');
}

// Parse questions from text format
function parseQuestions(text) {
    debug('Starting to parse questions');
    
    // Split text into lines
    const lines = text.split('\n');
    
    debug(`Found ${lines.length} question lines`);
    return lines.map((line, index) => {
        // Split by pipe
        const parts = line.split('|');
        
        if (parts.length < 8) {
            console.error(`Invalid question format at line ${index + 1}:`, line);
            return null;
        }
        
        const [id, questionText, ...rest] = parts;
        const options = rest.slice(0, 4);
        const correct = parseInt(rest[4]) - 1; // Convert from 1-based to 0-based indexing
        const explanation = rest[5];
        
        if (isNaN(correct) || correct < 0 || correct >= 4) {
            console.error(`Invalid correct answer at line ${index + 1}:`, correct);
            return null;
        }
        
        return {
            id: parseInt(id),
            text: questionText,
            options: options,
            correct,
            explanation: explanation
        };
    }).filter(q => q !== null);
}

// Render current question
function renderQuestion() {
    if (!questions || questions.length === 0) {
        showError('No questions available');
        return;
    }

    const container = document.getElementById('question-container');
    if (!container) return;

    const question = questions[currentQuestion];
    
    container.innerHTML = `
        <div class="question-header">
            <h3>Question ${currentQuestion + 1} of ${questions.length}</h3>
            <div class="question-status">${getQuestionStatus()}</div>
        </div>
        <div class="question-text">${question.text}</div>
        <div class="options">
            ${question.options.map((option, index) => {
                const isSelected = userAnswers[currentQuestion] === index;
                return `
                    <div class="option ${isSelected ? 'selected' : ''}" data-index="${index}">
                        <input type="radio" name="answer" value="${index}" ${isSelected ? 'checked' : ''}>
                        <span class="option-text">${option}</span>
                    </div>
                `;
            }).join('')}
        </div>
    `;

    // Add click handlers for options
    const options = container.querySelectorAll('.option');
    options.forEach(option => {
        option.addEventListener('click', () => {
            const index = parseInt(option.dataset.index);
            userAnswers[currentQuestion] = index;
            renderQuestion(); // Re-render to update selection
            buildQuestionPalette(); // Update question palette
            updateButtonStates(); // Update navigation buttons
        });
    });

    updateButtonStates();
    buildQuestionPalette();
}

// Setup event listeners
function setupEventListeners() {
    const prevBtn = document.getElementById('prevBtn');
    const nextBtn = document.getElementById('nextBtn');
    const markBtn = document.getElementById('markBtn');
    const submitBtn = document.getElementById('submitBtn');

    if (prevBtn) prevBtn.addEventListener('click', () => navigate(-1));
    if (nextBtn) nextBtn.addEventListener('click', () => navigate(1));
    if (markBtn) markBtn.addEventListener('click', toggleMarkQuestion);
    if (submitBtn) submitBtn.addEventListener('click', submitExam);
}

// Navigate between questions
function navigate(direction) {
    const newIndex = currentQuestion + direction;
    if (newIndex >= 0 && newIndex < questions.length) {
        currentQuestion = newIndex;
        renderQuestion();
    }
}

// Update button states
function updateButtonStates() {
    const prevBtn = document.getElementById('prevBtn');
    const nextBtn = document.getElementById('nextBtn');
    const submitBtn = document.getElementById('submitBtn');
    
    if (prevBtn) {
        prevBtn.disabled = currentQuestion === 0;
    }
    
    if (nextBtn) {
        nextBtn.disabled = currentQuestion === questions.length - 1;
    }
    
    // The submit button should always be enabled now
    // if (submitBtn) {
    //     const allAnswered = userAnswers.every(answer => answer !== -1);
    //     submitBtn.disabled = !allAnswered;
    // }
}

// Toggle mark for review
function toggleMarkQuestion() {
    if (markedQuestions.has(currentQuestion)) {
        markedQuestions.delete(currentQuestion);
    } else {
        markedQuestions.add(currentQuestion);
    }
    buildQuestionPalette();
}

// Build question palette
function buildQuestionPalette() {
    const palette = document.querySelector('.palette-buttons');
    if (!palette) return;
    
    palette.innerHTML = '';
    
    for (let i = 0; i < questions.length; i++) {
        const btn = document.createElement('button');
        btn.textContent = i + 1;
        btn.className = 'palette-btn';
        
        if (userAnswers[i] !== -1) btn.classList.add('answered');
        if (markedQuestions.has(i)) btn.classList.add('marked');
        if (i === currentQuestion) btn.classList.add('current');
        
        btn.addEventListener('click', () => {
            currentQuestion = i;
            renderQuestion();
        });
        palette.appendChild(btn);
    }
}

// Start exam timer
function startTimer() {
    const timerDisplay = document.getElementById('timer');
    if (!timerDisplay) return;
    
    // Clear any existing timer
    if (timerInterval) {
        clearInterval(timerInterval);
    }
    
    // Reset timer
    timeLeft = 1800; // 30 minutes
    
    timerInterval = setInterval(() => {
        const minutes = Math.floor(timeLeft / 60);
        const seconds = timeLeft % 60;
        
        timerDisplay.textContent = `${minutes}:${seconds.toString().padStart(2, '0')}`;
        
        if (timeLeft <= 30) {
            timerDisplay.classList.add('warning');
        }
        
        if (timeLeft <= 0) {
            clearInterval(timerInterval);
            submitExam();
            return;
        }
        
        timeLeft--;
    }, 1000);
}

// Submit exam
async function submitExam() {
    try {
        debug('Submitting exam...');
        clearInterval(timerInterval);
        
        // Check if all questions are answered
        const unansweredQuestions = userAnswers.filter(answer => answer === -1).length;
        if (unansweredQuestions > 0) {
            const proceed = confirm(`You have ${unansweredQuestions} unanswered questions. Are you sure you want to submit?`);
            if (!proceed) {
                return;
            }
        }
        
        // Calculate score and prepare detailed results
        let score = 0;
        const questionDetails = questions.map((question, index) => {
            const isCorrect = userAnswers[index] === question.correct;
            if (isCorrect) score++;
            
            return {
                questionNumber: index + 1,
                questionText: question.text,
                options: question.options,
                userAnswer: userAnswers[index],
                correctAnswer: question.correct,
                isCorrect: isCorrect,
                explanation: question.explanation || 'No explanation provided'
            };
        });
        
        const unattemptedCount = userAnswers.filter(answer => answer === -1).length;

        // Save exam data
        const examData = {
            score,
            totalQuestions: questions.length,
            unattemptedCount,
            percentage: (score / questions.length) * 100,
            timeSpent: 1800 - timeLeft,
            questionDetails: questionDetails
        };
        
        // Debug logging
        console.log('Exam Data to save:', examData);
        
        // Save to localStorage and verify it was saved
        localStorage.setItem('examData', JSON.stringify(examData));
        const savedData = localStorage.getItem('examData');
        
        // Debug logging
        console.log('Saved Data retrieved:', savedData ? JSON.parse(savedData) : 'No data found');
        
        if (!savedData) {
            throw new Error('Failed to save exam data');
        }
        
        // Ensure data is properly saved before redirecting
        const parsedData = JSON.parse(savedData);
        if (!parsedData.score || !parsedData.totalQuestions || !parsedData.questionDetails) {
            throw new Error('Exam data is incomplete');
        }
        
        // Redirect to scorecard
        console.log('Redirecting to scorecard...');
        window.location.href = 'scorecard.html';
    } catch (error) {
        console.error('Error in submitExam:', error);
        showError('Failed to submit exam: ' + error.message);
    }
}

// Initialize when DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
    debug('DOM loaded, initializing exam...');
    setupEventListeners();
    loadQuestions();
});