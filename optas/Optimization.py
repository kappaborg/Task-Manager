import numpy as np
import nnfs
from nnfs.datasets import vertical_data
import matplotlib.pyplot as plt

nnfs.init()

# Dense layer
class Layer_Dense:
    # Layer initialization
    def __init__(self, n_inputs, n_neurons):
        # Initialize weights and biases
        self.weights = 0.01 * np.random.randn(n_inputs, n_neurons)
        self.biases = np.zeros((1, n_neurons))
    
    # Forward pass
    def forward(self, inputs):
        # Calculate output values from inputs, weights and biases
        self.output = np.dot(inputs, self.weights) + self.biases

# ReLU activation
class Activation_ReLU:
    # Forward pass
    def forward(self, inputs):
        # Calculate output values from inputs
        self.output = np.maximum(0, inputs)

# Softmax activation
class Activation_Softmax:
    # Forward pass
    def forward(self, inputs):
        # Get unnormalized probabilities
        exp_values = np.exp(inputs - np.max(inputs, axis=1, keepdims=True))
        # Normalize them for each sample
        probabilities = exp_values / np.sum(exp_values, axis=1, keepdims=True)
        self.output = probabilities

# Common loss class
class Loss:
    # Calculates the data and regularization losses
    # given model output and ground truth values
    def calculate(self, output, y):
        # Calculate sample losses
        sample_losses = self.forward(output, y)
        # Calculate mean loss
        data_loss = np.mean(sample_losses)
        # Return loss
        return data_loss

# Cross-entropy loss
class Loss_CategoricalCrossentropy(Loss):
    # Forward pass
    def forward(self, y_pred, y_true):
        # Number of samples in a batch
        samples = len(y_pred)
        # Clip data to prevent division by 0
        # Clip both sides to not drag mean towards any value
        y_pred_clipped = np.clip(y_pred, 1e-7, 1-1e-7)

        # Probabilities for target values - 
        # only if categorical labels
        if len(y_true.shape) == 1:
            correct_confidences = y_pred_clipped[range(samples), y_true]
        # Mask values - only for one-hot encoded labels
        elif len(y_true.shape) == 2:
            correct_confidences = np.sum(y_pred_clipped*y_true, axis=1)

        # Losses
        negative_log_likelihoods = -np.log(correct_confidences)
        return negative_log_likelihoods

# Create dataset
X, y = vertical_data(samples=100, classes=3)

# Create model
dense1 = Layer_Dense(2, 3)  # first dense layer, 2 inputs
activation1 = Activation_ReLU()
dense2 = Layer_Dense(3, 3)  # second dense layer, 3 inputs, 3 outputs
activation2 = Activation_Softmax()

# Create loss function
loss_function = Loss_CategoricalCrossentropy()

# Helper variables
lowest_loss = 9999999  # some initial value
best_dense1_weights = dense1.weights.copy()
best_dense1_biases = dense1.biases.copy()
best_dense2_weights = dense2.weights.copy()
best_dense2_biases = dense2.biases.copy()

# Save initial parameters for visualization
initial_dense1_weights = dense1.weights.copy()
initial_dense1_biases = dense1.biases.copy()
initial_dense2_weights = dense2.weights.copy()
initial_dense2_biases = dense2.biases.copy()

# Perform a forward pass with initial parameters to get initial predictions
dense1.forward(X)
activation1.forward(dense1.output)
dense2.forward(activation1.output)
activation2.forward(dense2.output)
initial_predictions = np.argmax(activation2.output, axis=1)
initial_loss = loss_function.calculate(activation2.output, y)
initial_accuracy = np.mean(initial_predictions == y)

print("Training started")
for iteration in range(10000):
    # Update weights with some small random values
    dense1.weights += 0.05 * np.random.randn(2, 3)
    dense1.biases += 0.05 * np.random.randn(1, 3)
    dense2.weights += 0.05 * np.random.randn(3, 3)
    dense2.biases += 0.05 * np.random.randn(1, 3)
    
    # Perform a forward pass of our training data through this layer
    dense1.forward(X)
    activation1.forward(dense1.output)
    dense2.forward(activation1.output)
    activation2.forward(dense2.output)
    
    # Perform a forward pass through activation function
    # it takes the output of second dense layer here and returns loss
    loss = loss_function.calculate(activation2.output, y)
    
    # Calculate accuracy from output of activation2 and targets
    # calculate values along first axis
    predictions = np.argmax(activation2.output, axis=1)
    accuracy = np.mean(predictions == y)
    
    # If loss is smaller - print and save weights and biases aside
    if loss < lowest_loss:
        print(f'New set of weights found, iteration: {iteration}, loss: {loss:.4f}, acc: {accuracy:.4f}')
        best_dense1_weights = dense1.weights.copy()
        best_dense1_biases = dense1.biases.copy()
        best_dense2_weights = dense2.weights.copy()
        best_dense2_biases = dense2.biases.copy()
        lowest_loss = loss
    # Revert weights and biases
    else:
        dense1.weights = best_dense1_weights.copy()
        dense1.biases = best_dense1_biases.copy()
        dense2.weights = best_dense2_weights.copy()
        dense2.biases = best_dense2_biases.copy()

print("\nTraining complete!")
print(f"Best loss: {lowest_loss:.4f}")

# Perform a forward pass with the best parameters to get final predictions
dense1.weights = best_dense1_weights.copy()
dense1.biases = best_dense1_biases.copy()
dense2.weights = best_dense2_weights.copy()
dense2.biases = best_dense2_biases.copy()

dense1.forward(X)
activation1.forward(dense1.output)
dense2.forward(activation1.output)
activation2.forward(dense2.output)
final_predictions = np.argmax(activation2.output, axis=1)
final_accuracy = np.mean(final_predictions == y)

print(f"\nPerformance Comparison:")
print(f"Initial accuracy: {initial_accuracy:.4f}, Initial loss: {initial_loss:.4f}")
print(f"Final accuracy: {final_accuracy:.4f}, Final loss: {lowest_loss:.4f}")

# Visualize the data and predictions
plt.figure(figsize=(15, 5))

# Plot the original data
plt.subplot(1, 3, 1)
plt.scatter(X[:, 0], X[:, 1], c=y, cmap='brg')
plt.colorbar()
plt.title('Original Dataset')
plt.xlabel('Feature 1')
plt.ylabel('Feature 2')

# Plot initial predictions
plt.subplot(1, 3, 2)
plt.scatter(X[:, 0], X[:, 1], c=initial_predictions, cmap='brg')
plt.colorbar()
plt.title(f'Initial Predictions (Acc: {initial_accuracy:.4f})')
plt.xlabel('Feature 1')
plt.ylabel('Feature 2')

# Plot final predictions
plt.subplot(1, 3, 3)
plt.scatter(X[:, 0], X[:, 1], c=final_predictions, cmap='brg')
plt.colorbar()
plt.title(f'Final Predictions (Acc: {final_accuracy:.4f})')
plt.xlabel('Feature 1')
plt.ylabel('Feature 2')

plt.tight_layout()
plt.show()