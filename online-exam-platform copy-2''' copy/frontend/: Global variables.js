/*// Global variables
let currentQuestion = 0;
let questions = [];
let timeLeft = 1800; // 30 minutes in seconds
let timerInterval;

// DOM Elements
const timerElement = document.getElementById('timer');
const questionContainer = document.getElementById('question-container');
const prevBtn = document.getElementById('prevBtn');
const nextBtn = document.getElementById('nextBtn');

// Initialize exam
function initExam() {
    loadQuestions();
    startTimer();
    renderQuestion();
    
    // Event listeners
    prevBtn.addEventListener('click', () => navigate(-1));
    nextBtn.addEventListener('click', () => navigate(1));
}

// Timer functionality
function startTimer() {
    timerInterval = setInterval(() => {
        timeLeft--;
        updateTimerDisplay();
        
        if(timeLeft <= 0) {
            submitExam();
        }
    }, 1000);
}

function updateTimerDisplay() {
    const minutes = Math.floor(timeLeft / 60);
    const seconds = timeLeft % 60;
    timerElement.textContent = `${minutes}:${seconds.toString().padStart(2, '0')}`;
    
    if(timeLeft <= 30) {
        timerElement.classList.add('blink');
        timerElement.style.color = '#ff0000';
    }
}

// Question navigation
function navigate(direction) {
    // Save current answer
    saveAnswer();
    
    // Update question index
    currentQuestion += direction;
    
    // Boundary checks
    if(currentQuestion < 0) currentQuestion = 0;
    if(currentQuestion >= questions.length) currentQuestion = questions.length - 1;
    
    renderQuestion();
}

function renderQuestion() {
    const q = questions[currentQuestion];
    questionContainer.innerHTML = `
        <h3>Question ${currentQuestion + 1}</h3>
        <p>${q.text}</p>
        <form id="answer-form">
            ${q.options.map((opt, i) => `
                <label>
                    <input type="radio" name="answer" value="${i}" 
                        ${q.userAnswer === i ? 'checked' : ''}>
                    ${opt}
                </label><br>
            `).join('')}
        </form>
    `;
}

// Initialize when page loads
if(document.querySelector('.exam-container')) {
    initExam();
}





// script.js - Complete Updated Version

// =====================
// 1. LOGIN PAGE FUNCTIONALITY
// =====================
document.addEventListener('DOMContentLoaded', function() {
    // Check if we're on the login page
    if (document.getElementById('loginForm')) {
        const loginForm = document.getElementById('loginForm');
        const errorMsg = document.getElementById('errorMsg');

        loginForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            const username = document.getElementById('username').value.trim();
            const password = document.getElementById('password').value.trim();

            // Simple validation (replace with actual backend auth)
            if (username && password) {
                // For demo purposes - in real app, verify against backend
                if ((username === 'admin' && password === 'admin123') || 
                    (username === 'student' && password === 'student123')) {
                    window.location.href = 'instructions.html';
                } else {
                    errorMsg.textContent = 'Invalid username or password!';
                }
            } else {
                errorMsg.textContent = 'Please enter both username and password!';
            }
        });
    }

    // =====================
    // 2. INSTRUCTIONS PAGE
    // =====================
    if (document.getElementById('startExamBtn')) {
        const startBtn = document.getElementById('startExamBtn');
        const agreeCheckbox = document.getElementById('agreeCheckbox');

        agreeCheckbox.addEventListener('change', function() {
            startBtn.disabled = !this.checked;
        });

        startBtn.addEventListener('click', function() {
            window.location.href = 'exam.html';
        });
    }

    // =====================
    // 3. EXAM PAGE FUNCTIONALITY
    // =====================
    if (document.getElementById('timer')) {
        // Exam variables
        let currentQuestion = 0;
        let questions = [];
        let timeLeft = 1800; // 30 minutes in seconds
        let timerInterval;
        let userAnswers = {};
        const timerElement = document.getElementById('timer');

        // Initialize exam
        initExam();

        function initExam() {
            // Load sample questions (replace with actual API call)
            questions = [
                {
                    id: 1,
                    text: "What is 2+2?",
                    options: ["3", "4", "5", "6"],
                    correct: 1
                },
                {
                    id: 2,
                    text: "Capital of France?",
                    options: ["London", "Paris", "Berlin", "Madrid"],
                    correct: 1
                }
            ];

            startTimer();
            renderQuestion();
            setupEventListeners();
        }

        function startTimer() {
            updateTimerDisplay();
            timerInterval = setInterval(function() {
                timeLeft--;
                updateTimerDisplay();
                
                if (timeLeft <= 30) {
                    timerElement.classList.add('blink');
                    timerElement.style.color = '#ff0000';
                }
                
                if (timeLeft <= 0) {
                    submitExam();
                }
            }, 1000);
        }

        function updateTimerDisplay() {
            const minutes = Math.floor(timeLeft / 60);
            const seconds = timeLeft % 60;
            timerElement.textContent = `${minutes}:${seconds.toString().padStart(2, '0')}`;
        }

        function renderQuestion() {
            const q = questions[currentQuestion];
            const questionContainer = document.getElementById('question-container');
            
            questionContainer.innerHTML = `
                <div class="question">
                    <h3>Question ${currentQuestion + 1}</h3>
                    <p>${q.text}</p>
                    <form id="answerForm">
                        ${q.options.map((opt, i) => `
                            <label>
                                <input type="radio" name="answer" value="${i}" 
                                    ${userAnswers[currentQuestion] === i ? 'checked' : ''}>
                                ${opt}
                            </label><br>
                        `).join('')}
                    </form>
                </div>
            `;
            
            // Update question counter
            document.getElementById('current-q').textContent = currentQuestion + 1;
            document.getElementById('total-q').textContent = questions.length;
        }

        function setupEventListeners() {
            document.getElementById('prevBtn').addEventListener('click', function() {
                if (currentQuestion > 0) {
                    saveAnswer();
                    currentQuestion--;
                    renderQuestion();
                }
            });

            document.getElementById('nextBtn').addEventListener('click', function() {
                if (currentQuestion < questions.length - 1) {
                    saveAnswer();
                    currentQuestion++;
                    renderQuestion();
                }
            });

            document.getElementById('markBtn').addEventListener('click', function() {
                alert('Question marked for review!');
                // Implement mark for review logic
            });

            document.getElementById('submitBtn').addEventListener('click', function() {
                if (confirm('Are you sure you want to submit the exam?')) {
                    submitExam();
                }
            });
        }

        function saveAnswer() {
            const form = document.getElementById('answerForm');
            if (form) {
                const selected = form.querySelector('input[name="answer"]:checked');
                if (selected) {
                    userAnswers[currentQuestion] = parseInt(selected.value);
                }
            }
        }

        function submitExam() {
            clearInterval(timerInterval);
            saveAnswer();
            
            // Calculate score (replace with actual API call)
            let score = 0;
            for (const [qIndex, userAnswer] of Object.entries(userAnswers)) {
                if (userAnswer === questions[qIndex].correct) {
                    score++;
                }
            }
            
            // Store results temporarily (in real app, send to backend)
            localStorage.setItem('examResults', JSON.stringify({
                score: score,
                total: questions.length,
                answers: userAnswers
            }));
            
            window.location.href = 'scorecard.html';
        }
    }

    // =====================
    // 4. SCORECARD PAGE
    // =====================
    if (document.getElementById('resultsTable')) {
        const results = JSON.parse(localStorage.getItem('examResults')) || {
            score: 0,
            total: 10,
            answers: {}
        };

        // Display summary
        document.getElementById('correctCount').textContent = results.score;
        document.getElementById('wrongCount').textContent = results.total - results.score;
        document.getElementById('score').textContent = `${Math.round((results.score / results.total) * 100)}%`;

        // Populate results table (sample data)
        const tbody = document.querySelector('#resultsTable tbody');
        tbody.innerHTML = `
            <tr>
                <td>1</td>
                <td class="correct-answer">B</td>
                <td>B</td>
                <td>45s</td>
                <td><a href="#" class="solution-link">View Solution</a></td>
            </tr>
            <tr>
                <td>2</td>
                <td class="wrong-answer">A</td>
                <td>C</td>
                <td>30s</td>
                <td><a href="#" class="solution-link">View Solution</a></td>
            </tr>
        `;

        // Add event listeners for solution links
        document.querySelectorAll('.solution-link').forEach(link => {
            link.addEventListener('click', function(e) {
                e.preventDefault();
                const questionId = this.closest('tr').querySelector('td').textContent;
                alert(`Solution for question ${questionId} would be displayed here.`);
            });
        });
    }
});*/





// frontend/js/script.js
document.addEventListener('DOMContentLoaded', function() {
    // ======================
    // 1. LOGIN FUNCTIONALITY
    // ======================
    const loginForm = document.getElementById('loginForm');
    if (loginForm) {
        loginForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            const username = document.getElementById('username').value.trim();
            const password = document.getElementById('password').value.trim();
            const errorMsg = document.getElementById('errorMsg');

            // Clear previous errors
            errorMsg.textContent = '';

            // Client-side validation
            if (!username || !password) {
                errorMsg.textContent = 'Please enter both username and password';
                return;
            }

            // For demo purposes - accepts any non-empty credentials
            console.log('Login credentials:', { username, password });
            
            // Immediate redirect for testing
            // In production, replace with actual authentication:
            /*
            fetch('/api/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    window.location.href = 'instructions.html';
                } else {
                    errorMsg.textContent = data.message || 'Login failed';
                }
            })
            .catch(error => {
                console.error('Login error:', error);
                errorMsg.textContent = 'Connection error';
            });
            */
            
            // DEMO REDIRECT - REMOVE IN PRODUCTION
            window.location.href = 'instructions.html';
        });
    }

    // ======================
    // 2. INSTRUCTIONS PAGE
    // ======================
    const startExamBtn = document.getElementById('startExamBtn');
    if (startExamBtn) {
        document.getElementById('agreeCheckbox').addEventListener('change', function() {
            startExamBtn.disabled = !this.checked;
        });
        
        startExamBtn.addEventListener('click', function() {
            window.location.href = 'exam.html';
        });
    }
});