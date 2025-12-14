import { useState, useEffect } from "react"
import { Link } from "react-router-dom"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Eye, FileText, FilePenLine, Plus, Trash2 } from "lucide-react"
import { getStats, getAdminBlogs, deleteBlog, type BlogPost, type Stats } from "@/lib/api"

export default function DashboardPage() {
    const [stats, setStats] = useState<Stats | null>(null)
    const [blogs, setBlogs] = useState<BlogPost[]>([])
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        const fetchData = async () => {
            try {
                const [statsData, blogsData] = await Promise.all([
                    getStats(),
                    getAdminBlogs()
                ])
                setStats(statsData)
                setBlogs(blogsData)
            } catch (err) {
                console.error('Failed to fetch dashboard data:', err)
            } finally {
                setLoading(false)
            }
        }
        fetchData()
    }, [])

    const handleDelete = async (slug: string) => {
        // if (!confirm('Are you sure you want to delete this post?')) return
        try {
            await deleteBlog(slug)
            setBlogs(blogs.filter(b => b.url_slug !== slug))
        } catch (err) {
            console.error('Failed to delete:', err)
        }
    }

    return (
        <div className="p-8 space-y-8">
            <div className="flex items-center justify-between">
                <h1 className="text-3xl font-bold">Dashboard</h1>
                <Button asChild>
                    <Link to="/admin/editor">
                        <Plus className="mr-2 h-4 w-4" />
                        New Post
                    </Link>
                </Button>
            </div>

            {/* Analytics Cards */}
            <div className="grid gap-6 md:grid-cols-3">
                <Card>
                    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                        <CardTitle className="text-sm font-medium">Total Views</CardTitle>
                        <Eye className="h-4 w-4 text-muted-foreground" />
                    </CardHeader>
                    <CardContent>
                        <div className="text-2xl font-bold">{loading ? '...' : stats?.visits || 0}</div>
                    </CardContent>
                </Card>
                <Card>
                    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                        <CardTitle className="text-sm font-medium">Published Posts</CardTitle>
                        <FileText className="h-4 w-4 text-muted-foreground" />
                    </CardHeader>
                    <CardContent>
                        <div className="text-2xl font-bold">{loading ? '...' : stats?.published || 0}</div>
                    </CardContent>
                </Card>
                <Card>
                    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                        <CardTitle className="text-sm font-medium">Drafts</CardTitle>
                        <FilePenLine className="h-4 w-4 text-muted-foreground" />
                    </CardHeader>
                    <CardContent>
                        <div className="text-2xl font-bold">{loading ? '...' : stats?.drafts || 0}</div>
                    </CardContent>
                </Card>
            </div>

            {/* Blog List */}
            <div className="space-y-4">
                <h2 className="text-xl font-semibold">All Posts</h2>
                {loading ? (
                    <p className="text-muted-foreground">Loading...</p>
                ) : blogs.length === 0 ? (
                    <p className="text-muted-foreground">No posts yet. Create your first post!</p>
                ) : (
                    <div className="grid gap-4">
                        {blogs.map(blog => (
                            <Card key={blog.id} className="flex items-center justify-between p-4">
                                <div className="space-y-1">
                                    <div className="flex items-center gap-2">
                                        <h3 className="font-medium">{blog.title}</h3>
                                        <Badge variant={blog.status === 1 ? "default" : "secondary"}>
                                            {blog.status === 1 ? "Published" : "Draft"}
                                        </Badge>
                                    </div>
                                    <p className="text-sm text-muted-foreground">/blog/{blog.url_slug}</p>
                                </div>
                                <div className="flex gap-2">
                                    <Button variant="outline" size="sm" asChild>
                                        <Link to={`/admin/editor/${blog.url_slug}`}>Edit</Link>
                                    </Button>
                                    <Button
                                        variant="destructive"
                                        size="sm"
                                        onClick={() => handleDelete(blog.url_slug)}
                                    >
                                        <Trash2 className="h-4 w-4" />
                                    </Button>
                                </div>
                            </Card>
                        ))}
                    </div>
                )}
            </div>
        </div>
    )
}

