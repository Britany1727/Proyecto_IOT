import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. Cargar el archivo de datos consolidado
df = pd.read_csv('feed.csv')

# 2. Renombrar columnas para usar tus variables exactas
df = df.rename(columns={
    'field1': 'Banda',
    'field2': 'Temperatura',
    'field3': 'Color',
    'field4': 'Luz'
})

# 3. Preparación de datos para las gráficas de barras
color_map = {1: 'Rojo', 2: 'Verde', 3: 'Azul', 4: 'Descarte'}
df['Var_Color'] = df['Color'].map(color_map)

# Agrupamos los valores numéricos de Temp y Luz en rangos para las barras
df['Var_Temp'] = pd.cut(df['Temperatura'], bins=[0, 23, 26, 100], labels=['22-23°C', '24-26°C', '27-28°C'])
df['Var_Luz'] = pd.cut(df['Luz'], bins=[0, 450, 550, 1000], labels=['400-450', '500-550', '600-650'])

# 4. Generación del tablero de BARRAS (Únicamente lo solicitado)
plt.figure(figsize=(15, 10))

# --- Gráfica 1: Reporte Total de Clasificación ---
plt.subplot(2, 2, 1)
sns.countplot(data=df, x='Var_Color', palette=['red', 'green', 'blue', 'gray'])
plt.title('Reporte Total de Clasificación')
plt.ylabel('Cantidad de Cajas')

# --- Gráfica 2: Distribución de Temperatura Operativa ---
plt.subplot(2, 2, 2)
sns.countplot(data=df, x='Var_Temp', palette='coolwarm')
plt.title('Distribución de Temperatura Operativa')
plt.ylabel('Frecuencia de Registros')

# --- Gráfica 3: Variable Estado de Banda ---
plt.subplot(2, 2, 3)
sns.countplot(data=df, x='Banda', palette='viridis')
plt.title('Variable: Estado de Banda (Activaciones)')
plt.xticks([0, 1], ['Apagada', 'Activa'])
plt.ylabel('Conteo')

# --- Gráfica 4: Variable Intensidad de Luz ---
plt.subplot(2, 2, 4)
sns.countplot(data=df, x='Var_Luz', palette='YlGnBu')
plt.title('Variable: Intensidad de Luz Detectada')
plt.ylabel('Conteo')

plt.tight_layout()
plt.show()