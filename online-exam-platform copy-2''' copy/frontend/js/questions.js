// Function to parse questions from text format
function parseQuestions(text) {
    return text.split('\n').map(line => {
        const [id, text, ...rest] = line.split('|');
        const options = rest.slice(0, -2); // Remove correct answer and explanation
        const correct = parseInt(rest[rest.length - 2]) - 1; // Convert to 0-based index
        const explanation = rest[rest.length - 1]; // Get the explanation
        
        return {
            id: parseInt(id),
            text,
            options,
            correct,
            explanation
        };
    });
}

// Function to fetch questions from file
async function fetchQuestions() {
    try {
        const response = await fetch('/backend/questions.txt');
        if (!response.ok) {
            throw new Error('Failed to load questions');
        }
        const text = await response.text();
        return parseQuestions(text);
    } catch (error) {
        console.error('Error loading questions:', error);
        return [];
    }
}

// Function to get random questions
async function getRandomQuestions(count = 5) {
    const questions = await fetchQuestions();
    if (questions.length === 0) return [];
    
    if (count > questions.length) count = questions.length;
    
    // Shuffle and return requested number of questions
    return [...questions]
        .sort(() => 0.5 - Math.random())
        .slice(0, count);
}

// Export for use in other files
window.getRandomQuestions = getRandomQuestions;