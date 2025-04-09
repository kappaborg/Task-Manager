# GitHub Setup for TaskManager

Follow these steps to create a new GitHub repository and push the TaskManager project:

## 1. Create a New Repository on GitHub

1. Log in to your GitHub account
2. Click the "+" icon in the top-right corner and select "New repository"
3. Fill in the repository details:
   - Name: `TaskManager`
   - Description: `A comprehensive process management and task scheduling tool for Unix-like systems`
   - Visibility: Public (or Private if you prefer)
   - Initialize with: Do not select any options (no README, .gitignore, or license yet)
4. Click "Create repository"

## 2. Initialize Git in Your Local Project (if not already initialized)

```bash
cd /path/to/TaskManager
git init
```

## 3. Add and Commit Your Files

```bash
# Add all files to staging
git add .

# Commit the changes with a descriptive message
git commit -m "Initial commit: TaskManager project with process management and task scheduling capabilities"
```

## 4. Connect Your Local Repository to the GitHub Repository

```bash
# Replace YOUR_USERNAME with your actual GitHub username
git remote add origin https://github.com/YOUR_USERNAME/TaskManager.git
```

## 5. Push Your Code to GitHub

```bash
git push -u origin main
```

Note: If you're using an older version of Git, you might need to use `master` instead of `main`:

```bash
git push -u origin master
```

## 6. Add the Screen Recording to the Repository

After pushing the code:

1. Record a demonstration video showing:
   - Creating new tasks
   - Listing tasks
   - Starting the scheduler
   - Finding task PIDs
   - Changing task priorities

2. Save the screen recording to a video file (e.g., `demo.mp4`)

3. Add the video to your GitHub repository:
   - You can either commit and push the video file directly:
     ```bash
     git add demo.mp4
     git commit -m "Add demonstration video"
     git push
     ```
   - Or add it through the GitHub web interface by navigating to your repository and clicking "Add file" > "Upload files"

4. Update the README.md to include a link to the video:
   ```markdown
   ## Demonstration
   
   Watch a demonstration of TaskManager in action:
   
   [View Demo Video](./demo.mp4)
   ```

## 7. Finalize Your Repository

Make sure your repository includes:
- All source code
- usermanual.md
- README.md (with updated information)
- The demonstration video
- Any other necessary documentation

## 8. Share Your Repository

You can now share the URL of your repository (`https://github.com/YOUR_USERNAME/TaskManager`) with others so they can view, clone, or contribute to your project. 