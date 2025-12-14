import { Link, Outlet } from "react-router-dom"
import { LayoutDashboard, FileEdit, Settings, LogOut } from "lucide-react"
import { Button } from "@/components/ui/button"

export default function AdminLayout() {
    return (
        <div className="min-h-screen bg-muted/40 flex">
            {/* Sidebar */}
            <aside className="w-64 border-r bg-background hidden md:flex flex-col">
                <div className="h-16 flex items-center px-6 border-b">
                    <span className="font-bold text-lg">Admin Panel</span>
                </div>
                <nav className="flex-1 p-4 space-y-2">
                    <Button variant="ghost" className="w-full justify-start" asChild>
                        <Link to="/admin/dashboard">
                            <LayoutDashboard className="mr-2 h-4 w-4" />
                            Dashboard
                        </Link>
                    </Button>
                    <Button variant="ghost" className="w-full justify-start" asChild>
                        <Link to="/admin/editor">
                            <FileEdit className="mr-2 h-4 w-4" />
                            New Post
                        </Link>
                    </Button>
                    <Button variant="ghost" className="w-full justify-start" asChild>
                        <Link to="/admin/settings">
                            <Settings className="mr-2 h-4 w-4" />
                            Settings
                        </Link>
                    </Button>
                </nav>
                <div className="p-4 border-t">
                    <Button variant="outline" className="w-full" asChild>
                        <Link to="/">
                            <LogOut className="mr-2 h-4 w-4" />
                            Exit to Site
                        </Link>
                    </Button>
                </div>
            </aside>

            {/* Main Content */}
            <main className="flex-1 flex flex-col">
                <header className="h-16 border-b bg-background flex items-center justify-between px-6 md:hidden">
                    <span className="font-bold">Admin</span>
                    {/* Mobile Menu trigger would go here */}
                </header>
                <div className="flex-1 overflow-auto">
                    <Outlet />
                </div>
            </main>
        </div>
    )
}
