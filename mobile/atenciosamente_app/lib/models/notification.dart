class AppNotification {
  final int id;
  final String title;
  final String body;
  final DateTime createdAt;

  const AppNotification({
    required this.id,
    required this.title,
    required this.body,
    required this.createdAt,
  });

  factory AppNotification.fromJson(Map<String, dynamic> json) {
    return AppNotification(
      id: json['id'] as int,
      title: json['title'] as String,
      body: json['body'] as String,
      createdAt: DateTime.parse(json['created_at'] as String),
    );
  }

  // toJson() is deliberately NOT the mirror image of fromJson(). The backend's
  // create contract (POST /notifications) only accepts {title, body} — id and
  // createdAt are server-generated and don't exist yet at the point we're
  // sending this. Emitting only the fields the backend actually wants (and
  // rejects anything else) keeps this method honest about what it's for,
  // rather than serializing fields the server would ignore.
  Map<String, dynamic> toJson() {
    return {'title': title, 'body': body};
  }
}
