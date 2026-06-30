# LCR-ChassisManager-Repo

# Using RESTful API

Run this command to Log In:

`curl -c cjar -b cjar -k -H "Content-Type: application/json" -X POST -d '{"data": ["root", "0penBmc"]}' https://192.168.0.104/login`



Run commands like this to see data and schemas:

`curl -b cjar -k https://192.168.0.104/redfish/v1/Managers/bmc`

# Using RMCP

`ipmitool -I lanplus -H 192.168.0.104 -U root -P 0penBmc -C 17 mc info`


# Example process for development in this repo

New features should be added in a clean manner to avoid unnecessary git history. As such, branches for new features must be created, and changes must be tracked with 'amend' git arguments. Therefore, when 
the feature is merged back into the main branch, the commit history will only show the finished feature additions to the repository. The goal is to avoid many "compilation fix" git commits.

### Step 1: Start branch
`git checkout -b fix-yocto-recipe`

### Edit files, e.g., recipes/myapp/myapp.bb

### Step 2: Commit and push
```
git add recipes/myapp/myapp.bb
git commit -m "WIP: Add new dependency"
git push origin fix-yocto-recipe  # Triggers remote bitbake my-image
```

### Build fails remotely (e.g., missing header)

### Step 4: Fix and amend
### Edit to add DEPENDS += "my-header-package"
```
git add recipes/myapp/myapp.bb
git commit --amend --no-edit
```

### Step 5: Force-push
`git push origin fix-yocto-recipe --force-with-lease  # Re-triggers build`

### Repeat if needed...

### Step 6: Finalize
```
git commit --amend -m "Fixed recipe dependency for successful build"
git push origin fix-yocto-recipe --force-with-lease
git checkout main
git merge fix-yocto-recipe
git push origin main
git branch -d fix-yocto-recipe
git push origin --delete fix-yocto-recipe
```