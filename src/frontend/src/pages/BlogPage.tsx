import { useState, useEffect } from "react"
import { Link } from "react-router-dom"
import { getPublishedBlogs, type BlogPost } from "@/lib/api"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"
import { MarkdownExcerpt } from "@/components/markdown/MarkdownExcerpt"
import { Calendar, ArrowRight } from "lucide-react"

export default function BlogPage() {
    const [blogs, setBlogs] = useState<BlogPost[]>([])
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        const fetchBlogs = async () => {
            try {
                const data = await getPublishedBlogs()
                setBlogs(data)
            } catch (err) {
                console.error('Failed to fetch blogs:', err)
            } finally {
                setLoading(false)
            }
        }
        fetchBlogs()
    }, [])

    return (
        <div className="container py-10">
            <div className="max-w-3xl mx-auto">
                <h1 className="text-4xl font-bold mb-2">Blog</h1>
                <p className="text-muted-foreground mb-8">
                    Thoughts on technology, programming, and life.
                </p>

                {loading ? (
                    <p className="text-muted-foreground">Loading...</p>
                ) : blogs.length === 0 ? (
                    <p className="text-muted-foreground">No posts yet. Check back soon!</p>
                ) : (
                    <div className="grid gap-6">
                        {blogs.map(blog => {
                            const date = new Date(blog.publish_date * 1000).toLocaleDateString('en-US', {
                                year: 'numeric',
                                month: 'long',
                                day: 'numeric'
                            })
                            const tags = blog.tags ? blog.tags.split(',').map(t => t.trim()) : []

                            return (
                                <Link key={blog.id} to={`/blog/${blog.url_slug}`}>
                                    <Card className="hover:shadow-md transition-shadow">
                                        <CardHeader>
                                            <div className="flex items-center justify-between">
                                                <div className="flex items-center gap-2 text-sm text-muted-foreground">
                                                    <Calendar className="h-4 w-4" />
                                                    {date}
                                                </div>
                                                {blog.category && (
                                                    <Badge variant="secondary">{blog.category}</Badge>
                                                )}
                                            </div>
                                            <CardTitle className="text-xl group-hover:text-primary transition-colors">
                                                {blog.title}
                                            </CardTitle>
                                            <MarkdownExcerpt content={blog.summary || blog.description || blog.content} />
                                        </CardHeader>
                                        <CardContent>
                                            <div className="flex items-center justify-between">
                                                <div className="flex gap-2">
                                                    {tags.slice(0, 3).map(tag => (
                                                        <Badge key={tag} variant="outline" className="text-xs">
                                                            {tag}
                                                        </Badge>
                                                    ))}
                                                </div>
                                                <span className="text-sm text-primary flex items-center gap-1">
                                                    Read more <ArrowRight className="h-4 w-4" />
                                                </span>
                                            </div>
                                        </CardContent>
                                    </Card>
                                </Link>
                            )
                        })}
                    </div>
                )}
            </div>
        </div>
    )
}

