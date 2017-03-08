mkdir smar2c
cd smar2c
mkdir iotsummer
cd iotsummer
git remote add origin https://{nombre-de-usuario-de-bitbucket}@bitbucket.org/smar2c/iotsummer.git

echo "{mi-nombre-y-apellido}" >> contributors.txt
git add contributors.txt
git commit -m 'Initial commit with contributor {nombre}'
git push -u origin master