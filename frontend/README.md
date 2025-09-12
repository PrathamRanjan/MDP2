# 🖥️ MDP Frontend - Robot Path Planning Simulator

<div align="center">
  <h2>React-based Web Interface for Robot Path Planning</h2>
  <p><em>Modern, responsive simulator with real-time path visualization</em></p>
  
  ![React](https://img.shields.io/badge/React-18.2.0-blue)
  ![Next.js](https://img.shields.io/badge/Next.js-13.3.1-black)
  ![TailwindCSS](https://img.shields.io/badge/TailwindCSS-3.3.1-38B2AC)
</div>

## 📋 Overview

The frontend provides an intuitive web-based interface for the MDP 25/26 robot path planning simulator. Built with React and Next.js, it offers real-time visualization of the robot's movement through a 20×20 grid environment.

## ✨ Key Features

### **🎮 Interactive Controls**
- **Robot Positioning**: Set X, Y coordinates and direction
- **Obstacle Placement**: Click-to-add obstacles with directional symbols
- **Real-time Preview**: Live grid updates as you modify the environment

### **📊 Path Visualization**  
- **Step-by-step Navigation**: Forward/backward through computed path
- **Command Display**: Shows robot movement instructions (FW, BW, SNAP, etc.)
- **Distance Tracking**: Real-time path length calculations
- **Progress Indicator**: Current step vs. total steps

### **🎨 Modern UI Design**
- **Dark Theme**: Gradient backgrounds with neon cyan/emerald accents
- **Responsive Layout**: Works on desktop, tablet, and mobile devices
- **Smooth Animations**: Hover effects and transitions throughout
- **Visual Feedback**: Clear status indicators and error handling

## 🏗️ Architecture

```
frontend/
├── components/
│   ├── Simulator.js      # Main simulator component
│   ├── BaseAPI.js        # HTTP client configuration  
│   └── QueryAPI.js       # Backend API interface
├── pages/
│   ├── _app.js          # Next.js app configuration
│   └── _document.js     # HTML document structure
├── styles/
│   └── globals.css      # Global CSS styles
├── package.json         # Dependencies and scripts
├── tailwind.config.js   # TailwindCSS configuration
└── next.config.js       # Next.js build configuration
```

## 🚀 Quick Start

### **Prerequisites**
- Node.js 16.0 or higher
- npm or yarn package manager

### **Installation**
```bash
# Install dependencies
npm install

# Start development server
npm run dev

# Build for production
npm run build

# Start production server  
npm start
```

The application will be available at `http://localhost:3000`

## 🔧 Configuration

### **Backend Connection**
Update the backend URL in `components/BaseAPI.js`:
```javascript
const host = "http://localhost:5001/";  // Backend server address
```

### **UI Customization**
Modify colors and styling in `tailwind.config.js` and component files.

## 📡 API Integration

### **Path Planning Request**
```javascript
// Example API call structure
{
  "obstacles": [
    {"x": 10, "y": 10, "id": 1, "d": 0}  // North-facing obstacle
  ],
  "robot_x": 1,
  "robot_y": 1, 
  "robot_dir": 0,          // North = 0, East = 2, South = 4, West = 6
  "retrying": false        // Optimization mode
}
```

### **Response Handling**
```javascript
// Received path data structure
{
  "data": {
    "commands": ["FW90", "SNAP1_C", "FIN"],
    "distance": 46.0,
    "path": [
      {"x": 1, "y": 1, "d": 0, "s": -1},
      {"x": 10, "y": 14, "d": 4, "s": 1}
    ]
  },
  "error": null
}
```

## 🎯 Component Details

### **Simulator.js - Main Component**
- **Grid Rendering**: 20×20 table with coordinate system
- **Robot Visualization**: 3×3 robot footprint with direction indicator  
- **Obstacle Management**: Add/remove obstacles with direction symbols
- **Path Execution**: Step-through computed robot movements

### **BaseAPI.js - HTTP Client**
- **Request Configuration**: JSON headers and CORS handling
- **Error Management**: Comprehensive error catching and reporting
- **Response Processing**: Automatic JSON parsing and validation

### **QueryAPI.js - Backend Interface**  
- **Path Planning API**: Sends obstacle/robot data to backend
- **Callback Handling**: Async response processing
- **Data Transformation**: Frontend to backend data format conversion

## 🎨 UI Components & Styling

### **Color Scheme**
```css
/* Primary Gradients */
background: gradient(indigo-900 → purple-800 → pink-700)
panels: gradient(slate-800 → slate-700) 
accents: cyan-400, emerald-500, purple-500

/* Interactive Elements */  
buttons: gradient backgrounds with hover effects
inputs: dark slate with cyan borders
text: cyan/emerald on dark backgrounds
```

### **Responsive Design**
- **Mobile-first**: TailwindCSS responsive utilities
- **Breakpoints**: Optimized for sm/md/lg screen sizes  
- **Touch-friendly**: Large click targets and button spacing

## 🧪 Testing the Frontend

### **Basic Functionality**
1. **Grid Display**: Verify 20×20 grid renders correctly
2. **Robot Controls**: Test X/Y/Direction input validation
3. **Obstacle Placement**: Add obstacles at various coordinates  
4. **API Connection**: Submit requests and verify responses

### **Edge Cases**
- **Invalid Coordinates**: Out-of-bounds robot/obstacle positions
- **Empty Obstacles**: Submit with no obstacles placed
- **Network Errors**: Backend offline scenarios
- **Large Paths**: 5+ obstacle navigation

### **Visual Testing**
- **Cross-browser**: Chrome, Firefox, Safari compatibility
- **Device Sizes**: Desktop, tablet, mobile responsiveness
- **Theme Consistency**: Dark theme across all components

## 📦 Dependencies

### **Core Framework**
- `react`: 18.2.0 - UI component library
- `react-dom`: 18.2.0 - DOM rendering
- `next`: 13.3.1 - React framework with SSR

### **Styling**
- `tailwindcss`: 3.3.1 - Utility-first CSS framework
- `daisyui`: 2.51.5 - TailwindCSS component library  
- `autoprefixer`: 10.4.14 - CSS vendor prefixes

### **Development**
- `eslint`: 8.39.0 - Code linting
- `eslint-config-next`: 13.3.1 - Next.js ESLint config

## 🚀 Deployment

### **Production Build**
```bash
npm run build      # Creates optimized production build
npm start          # Serves production build on port 3000
```

### **Environment Variables**
Create `.env.local` for environment-specific configuration:
```env
NEXT_PUBLIC_API_URL=http://localhost:5001
NEXT_PUBLIC_APP_NAME=MDP 25/26 Simulator
```

## 🔍 Troubleshooting

### **Common Issues**

**Backend Connection Failed**
- Verify backend is running on port 5001
- Check CORS configuration in Flask app
- Confirm API endpoint URLs match

**Grid Not Rendering**
- Check browser console for JavaScript errors  
- Verify all dependencies installed correctly
- Test with different browsers

**Slow Performance**
- Enable Next.js production mode
- Check for console warnings/errors
- Monitor network requests in DevTools

## 🤝 Contributing

When modifying the frontend:

1. **Component Structure**: Keep components small and focused
2. **Styling Consistency**: Use TailwindCSS classes consistently
3. **API Integration**: Handle all async operations properly
4. **Error Boundaries**: Implement comprehensive error handling
5. **Testing**: Verify cross-browser compatibility

## 📝 Future Enhancements

- **3D Visualization**: Three.js integration for 3D robot movement
- **Real-time Collaboration**: Multiple users planning simultaneously  
- **Advanced Analytics**: Path efficiency metrics and comparisons
- **Export Functionality**: Save configurations and replay sessions
- **WebSocket Integration**: Real-time robot status updates

---

<div align="center">
  <strong>🖥️ Modern Web Interface for Advanced Robot Path Planning 🚀</strong>
</div>