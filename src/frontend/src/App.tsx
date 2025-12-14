import { BrowserRouter, Routes, Route } from "react-router-dom"
import { AuthProvider } from "./lib/auth"
import { ProtectedRoute } from "./components/ProtectedRoute"
import Layout from "./components/layout/Layout"
import HomePage from "./pages/HomePage"
import BlogPage from "./pages/BlogPage"
import BlogDetailPage from "./pages/BlogDetailPage"
import LoginPage from "./pages/LoginPage"
import AuthCallbackPage from "./pages/AuthCallbackPage"
import AdminLayout from "./components/layout/AdminLayout"
import DashboardPage from "./pages/admin/DashboardPage"
import EditorPage from "./pages/admin/EditorPage"

function App() {
  return (
    <AuthProvider>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<Layout />}>
            <Route index element={<HomePage />} />
            <Route path="blog" element={<BlogPage />} />
            <Route path="blog/:slug" element={<BlogDetailPage />} />
            <Route path="login" element={<LoginPage />} />
            <Route path="auth" element={<AuthCallbackPage />} />
          </Route>

          {/* Admin Routes - Protected */}
          <Route path="/admin" element={
            <ProtectedRoute>
              <AdminLayout />
            </ProtectedRoute>
          }>
            <Route path="dashboard" element={<DashboardPage />} />
            <Route path="editor" element={<EditorPage />} />
            <Route path="editor/:slug" element={<EditorPage />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </AuthProvider>
  )
}

export default App


